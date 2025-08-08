/* vm.c: 가상 메모리 객체를 위한 일반적인 인터페이스 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"

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

// [구현1-1.해시테이블세팅] 해시 테이블 요소 추가
// 페이지 p에 대한 해시 값을 반환한다.
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED){
	// struct page의 시작 주소를 알아내서
	const struct page *p = hash_entry (p_, struct page, hash_elem);
	// p->va는 가상 주소 즉 키 역할, 키를 바탕으로 해시값을 계산해서 반환한다.
	return hash_bytes (&p->va, sizeof p->va);
}

// [구현1-1.해시테이블세팅] 해시 테이블 요소 추가
// 페이지 a가 페이지 b보다 앞서면 true를 반환한다.
// 목적 : 해시 충돌이 발생했을 때 같은 해시값을 갖는 요소들끼리 정렬된 순서로 저장하기 위함
bool
page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED){
	const struct page *a = hash_entry (a_, struct page, hash_elem);
	const struct page *b = hash_entry (b_, struct page, hash_elem);
	return a->va > b->va;
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

/* 헬퍼 함수들 */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* initializer를 통해 대기 중인 페이지 객체를 생성합니다.
 * 페이지를 직접 생성하지 말고, 이 함수나 `vm_alloc_page`를 통해 생성하세요. */
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

		/* TODO: 생성한 페이지를 spt에 삽입합니다. */
	}
err:
	return false;
}

// [구현 1-2] 보조페이지테이블 구현
/* spt에서 VA에 해당하는 페이지를 찾아 반환합니다.
 * 실패 시 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	// struct page *page = NULL;
	/* TODO: 이 함수를 구현하세요. */
	// 구조체 page 임시 객체, 이건 검색용도
	struct page p;
	struct hash_elem *e; 		// hash_find의 검색결과 저장용
	// 찾고자 하는 가상 주소를 p의 va필드에 넣음. 이 va가 해시의 key 역할
	p.va = va;
	e = hash_find(&spt -> pages, &p.hash_elem);

	// hash_find 반환값이 null이 아니면 page 꺼내서 반환
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

// [구현 1-2] 보조페이지테이블 구현
/* PAGE를 spt에 삽입합니다. 삽입 시 유효성 검사를 수행합니다. */
bool
spt_insert_page (struct supplemental_page_table *spt, struct page *page) {
	int succ = false;
	/* TODO: 이 함수를 구현하세요. */
	succ = hash_insert(&spt->pages, &page -> hash_elem);

	return succ;
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
 * 항상 유효한 주소를 반환해야 합니다. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: 이 함수를 구현하세요. */

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

	return vm_do_claim_page (page);
}

/* 페이지를 해제합니다.
 * 이 함수는 수정하지 마세요. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* 주어진 VA에 해당하는 페이지를 할당합니다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 구현하세요. */

	return vm_do_claim_page (page);
}

/* 주어진 PAGE를 할당하고 MMU를 설정합니다. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 링크 설정 */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지 테이블 엔트리를 추가하여 페이지의 VA와 프레임의 PA를 매핑합니다. */

	return swap_in (page, frame->kva);
}

// [구현 1-2] 보조페이지테이블 구현
/* 새로운 보조 페이지 테이블을 초기화합니다. */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
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
