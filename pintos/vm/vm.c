/* vm.c: 가상 메모리 객체를 위한 일반적인 인터페이스 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "vm/uninit.h"

/* 가상 메모리 서브시스템을 초기화합니다.
 * 각 서브시스템의 초기화 코드를 호출합니다. */
void
vm_init (void) {
	vm_anon_init ();   // 익명 페이지 초기화
	vm_file_init ();   // 파일 매핑 페이지 초기화
#ifdef EFILESYS  /* Project 4의 경우 */
	pagecache_init ();  // 페이지 캐시 초기화
#endif
	register_inspect_intr ();  // 디버깅용 인터럽트 등록
	/* 위의 줄은 수정하지 마시오. */
	/* TODO: 여기에 코드를 작성하세요. */
}

/* 페이지의 타입을 반환합니다.
 * 페이지가 초기화된 이후 어떤 타입인지 확인할 때 유용합니다.
 * 이 함수는 이미 구현되어 있습니다. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}
/* hash_bytes()를 사용해 가상 주소(va)를 기준으로 해시 키를 생성
 * hash_init() 함수는 각 요소를 해시 테이블에 넣을 때 고유한 키(hash value)를 요구
 * SPT는 가상 주소(void *va)를 기준으로 페이지를 식별 -> struct page의 va를 기준으로 해싱
 * 해시 테이블이 struct page들을 적절하게 분류(버킷 분배) */
unsigned
page_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct page *p = hash_entry(e, struct page, hash_elem); // hash_entry는 page 구조체의 hash_elem의 포인터(e)를 이용해
															// struct page 구조체의 포인터를 계산함.
    return hash_bytes(&p->va, sizeof p->va); // 키(key)값으로 va를 사용, 그 va를 해시 테이블로 사용할 수 있도록 해시 값을 생성
}

/* 두 struct page 객체를 가상 주소(va) 기준으로 작은지 비교
 * 해시 테이블은 내부적으로 충돌을 해결할 때 정렬된 순서를 필요로 하며, 중복 여부 판단에도 비교 함수가 필요
 * 즉, 해시 키가 같더라도 va가 다르면 다른 페이지로 인식해야 함
 * 정렬된 탐색 또는 hash_find()에서 사용되는 비교 기준이 필요 */ 
bool
page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct page *p_a = hash_entry(a, struct page, hash_elem);
    struct page *p_b = hash_entry(b, struct page, hash_elem);
    return p_a->va < p_b->va; 
}

/* 헬퍼 함수들 */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer를 통해 대기 중인 페이지 객체를 생성합니다.
 * 페이지를 직접 생성하지 말고, 이 함수나 `vm_alloc_page`를 통해 생성하세요. 
 이 함수는 초기화되지 않은 주어진 type의 페이지를 생성합니다. 
 초기화되지 않은 페이지의 swap_in 핸들러는 자동적으로 페이지 타입에 맞게 페이지를 초기화하고 
 주어진 AUX를 인자로 삼는 INIT 함수를 호출합니다. 
당신이 페이지 구조체를 가지게 되면 프로세스의 보조 페이지 테이블에 그 페이지를 삽입하십시오. 
vm.h에 정의되어 있는 VM_TYPE 매크로를 사용하면 편리할 것입니다.  
 */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* upage가 이미 사용 중인지 확인합니다. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 타입에 따라 initializer를 가져옵니다.
		 * TODO: 그런 다음 uninit_new를 호출하여 "uninit" 페이지 구조체를 생성합니다.
		 * TODO: uninit_new 호출 후 필요한 필드를 수정하세요. */
		struct page *page = (struct page *)malloc(sizeof(struct page)); // 페이지 만들고
		if(page == NULL){
			goto err;
		}
		uninit_new(page, upage, init, type, aux, uninit_initialize); // uninit_new 함수로 해당 페이지를 UNINIT 타입으로 초기화
		page->writable = writable; // writable 정보도 바꿔줌
		
		/* TODO: 생성한 페이지를 spt에 삽입합니다. */
		if(!spt_insert_page(spt, page)){ // 삽입
			free(page); // 삽입 실패하면 페이지 해제
			goto err;
		}
		return true;

	}
err:
	return false;
}

/* spt에서 VA에 해당하는 페이지를 찾아 반환합니다.
 * 실패 시 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 구현하세요. */
	struct page p; // 검색을 위한 임시 페이지 선언
	struct hash_elem *e; // 해쉬 테이블로 찾기 위해, hash_find의 검색 결과를 담는 포인터 선언

	p.va = pg_round_down(va); // 임시 페이지의 va값을 페이지 시작 주소로, 검색 key값이 될 페이지의 시작 주소임
	e = hash_find(&spt->pages, &p.hash_elem); // hash_find 함수를 통해 spt에서 일치하는 요소 찾기

	if (e != NULL){ // 검색에 성공하면
		page = hash_entry(e, struct page, hash_elem); // 페이지에 struct page의 주소를 삽입
	}

	return page; // 페이지 반환, 실패하면 87라인의 NULL 반환(if문 안 들어감)
}

bool insert_page(struct hash *pages, struct page *p){
	return hash_insert(pages, &p->hash_elem) == NULL;
}

/* PAGE를 spt에 삽입합니다. 삽입 시 유효성 검사를 수행합니다. 
이 함수는 인자로 주어진 보조 페이지 테이블에 페이지 구조체를 삽입합니다. 
이 함수에서 주어진 보충 테이블에서 가상 주소가 존재하지 않는지 검사해야 합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* TODO: 이 함수를 구현하세요. */
	return insert_page(&spt->pages, page);
}



/* 페이지를 spt에서 제거하고 메모리를 해제합니다. */
void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* 희생될 프레임을 선택합니다. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: 희생 페이지 선택 정책은 여러분이 정하세요. */

	return victim;
}

/* 하나의 페이지를 swap out 하고 해당 프레임을 반환합니다.
 * 실패 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 swap out 하고 해당 프레임을 반환합니다. */

	return NULL;
}

/* palloc()을 사용하여 프레임을 얻습니다.
 * 사용 가능한 페이지가 없다면, 프레임을 eviction 하여 메모리를 확보합니다.
 * 항상 유효한 주소를 반환해야 합니다. 
 * 
 * 이 함수는 palloc_get_page 함수를 호출함으로써 당신의 메모리 풀에서 새로운 물리메모리 페이지를 가져옵니다. 
 * 유저 메모리 풀에서 페이지를 성공적으로 가져오면, 프레임을 할당하고 프레임 구조체의 멤버들을 초기화한 후 해당 프레임을 반환합니다. 
 * 당신이 frame *vm_get_frame  함수를 구현한 후에는 모든 유저 공간 페이지들을 이 함수를 통해 할당해야 합니다.
 * 지금으로서는 페이지 할당이 실패했을 경우의 swap out을 할 필요가 없습니다. 
 * 일단 지금은 PANIC ("todo")으로 해당 케이스들을 표시해 두십시오.
*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: 이 함수를 구현하세요. */
	void *kva = palloc_get_page(PAL_USER); // 유저 메모리에서 물리메모리 페이지를 가져옴
	if(kva == NULL){ // 물리메모리 페이지를 가져오는 데 실패했을 경우
		PANIC("kva is NULL");
	}
	frame = (struct frame *)malloc(sizeof(struct frame));
	if(frame == NULL){
		palloc_free_page(kva);
		PANIC("frame is NULL");
	}

	frame->kva = kva; // 프레임 할당
	frame->page = NULL; // 프레임 구조체의 멤버들을 초기화

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 성장시킵니다. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* write-protected 페이지에 대한 fault를 처리합니다. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 성공 시 true를 반환합니다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: 접근 오류가 유효한지 확인합니다. */
	/* TODO: 여기에 코드를 작성하세요. */
    if (is_kernel_vaddr(addr) || !not_present) { // 커널 영역이거나, 페이지 자체가 없거나
        return false;
    }
    page = spt_find_page(spt, addr);
    if (page == NULL) {
        return false;
    }

    if (write && !page->writable) {
        return false;
    }

	return vm_do_claim_page (page);
}

/* 페이지를 해제합니다.
 * 이 함수는 수정하지 마세요. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* 주어진 VA에 해당하는 페이지를 할당합니다.
 위 함수는 인자로 주어진 va에 페이지를 할당하고, 해당 페이지에 프레임을 할당합니다. 
 당신은 우선 한 페이지를 얻어야 하고 그 이후에 해당 페이지를 인자로 갖는 vm_do_claim_page라는 함수를 호출해야 합니다.
 */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 구현하세요. */
	page = spt_find_page(&thread_current()->spt, (void *)va);

	if(page == NULL){
		return false;
	}


	return vm_do_claim_page (page);
}

/* 주어진 PAGE를 할당하고 MMU를 설정합니다. 
이 함수는 인자로 주어진 page에 물리 메모리 프레임을 할당합니다. 
당신은 먼저 vm_get_frame 함수를 호출함으로써 프레임 하나를 얻습니다(이 부분은 스켈레톤 코드에 구현되어 있습니다). 
그 이후 당신은 MMU를 세팅해야 하는데, 이는 가상 주소와 물리 주소를 매핑한 정보를 페이지 테이블에 추가해야 한다는 것을 의미합니다.
이 함수는 앞에서 말한 연산이 성공적으로 수행되었을 경우에 true를 반환하고 그렇지 않을 경우에 false를 반환합니다.*/
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	struct thread *curr = thread_current();

	/* 링크 설정 */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지 테이블 엔트리를 추가하여 페이지의 VA와 프레임의 PA를 매핑합니다. */
	//uint64_t PA = vtop(frame->kva);
	// 인자: 현재 프로세스의 페이지 테이블, 매핑할 가상 주소(어떤 가상 주소를 매핑할지), 
	// 매핑할 물리 메모리(어디 물리 메모리로 매핑할지), 쓰기 권한
	if(!pml4_set_page(curr->pml4, page->va, frame->kva, page->writable)){ 
		palloc_free_page(frame->kva); //get page에서 사용한 페이지 palloc 해제
		free(frame); // 마찬가지로 malloc frame 해제
		page->frame = NULL; //프레임 초기화
		return false;
	}


	return swap_in (page, frame->kva);
}

/* 새로운 보조 페이지 테이블을 초기화합니다. */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* 보조 페이지 테이블을 src로부터 dst로 복사합니다. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* 보조 페이지 테이블이 사용하는 자원을 해제합니다. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: 해당 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 저장소에 기록합니다. */
}
