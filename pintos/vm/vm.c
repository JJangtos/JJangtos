/* vm.c: 가상 메모리 객체를 위한 일반적인 인터페이스 */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

/* Project 3 */
struct list frame_table;
struct list_elem* start;

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
	list_init (&frame_table);
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
    struct page *p = hash_entry(e, struct page, hash_elem);
    return hash_bytes(&p->va, sizeof p->va);
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

/* 주어진 페이지 p를 해시 테이블 pages에 삽입
 * 삽입 성공 시 true, 이미 동일한 키가 존재해서 삽입 실패 시 false를 반환 */
bool insert_page(struct hash *pages, struct page *p){
    if(!hash_insert(pages, &p->hash_elem)) 
        return true;
    else
        return false;
}

/* 주어진 페이지 p를 해시 테이블 pages에 삭제 
 * 삭제 성공 시 true, 해당 항목이 이미 삭제 되어 있을 시 false를 반환 */
bool delete_page(struct hash *pages, struct page *p){
    if(!hash_delete(pages, &p->hash_elem))
        return true;
    else
        return false;
}

bool
install_page (void *upage, void *kpage, bool writable) {
    struct thread *curr = thread_current ();

    // upage는 유저 주소 공간이어야 하며, 페이지 단위로 정렬돼야 함
    ASSERT (pg_ofs (upage) == 0);
    ASSERT (is_user_vaddr (upage));
    ASSERT (kpage != NULL);

    // 이미 매핑된 페이지인지 확인
    if (pml4_get_page (curr->pml4, upage) == NULL) {
        // 매핑 시도: 성공하면 true, 실패하면 false
        return pml4_set_page (curr->pml4, upage, kpage, writable);
    }

    // 이미 매핑돼 있으면 실패
    return false;
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
		struct page* page = (struct page*)malloc(sizeof(struct page));

		typedef bool(*initializerFunc)(struct page *, enum vm_type, void *);
        initializerFunc initializer = NULL;

		switch(VM_TYPE(type)){
            case VM_ANON:
                initializer = anon_initializer;
                break;
            case VM_FILE:
                initializer = file_backed_initializer;
                break;
		}
		uninit_new(page, upage, init, type, aux, initializer);

		page->writable = writable;
		/* TODO: 생성한 페이지를 spt에 삽입합니다. */
		return spt_insert_page(spt,page);
	}
err:
	return false;
}

/* spt에서 VA에 해당하는 페이지를 찾아 반환합니다.
 * 실패 시 NULL을 반환합니다. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: 이 함수를 구현하세요. */
	struct page* page = (struct page*)malloc(sizeof(struct page)); // dummy page 생성
	struct hash_elem *e; // va가 가르키는 가상의 page의 시작 포인트 (offset이 0으로 설정된 va) 반환
	page->va=pg_round_down(va); //페이지 정렬: va를 페이지 단위로 내림(round down)하여, 페이지의 시작 주소로 설정. 이는 spt에 저장된 키(va)와 정확히 일치시키기 위함
	e = hash_find(&spt->pages, &page->hash_elem); // page->va를 기준으로 hash_elem을 조회, 내부적으로 page_hash와 page_less를 이용하여 비교
	free(page); // 더미 page는 더 이상 필요 없으므로 해제
	return e !=NULL ? hash_entry(e,struct page, hash_elem) : NULL; // e가 NULL이 아니면, e가 가리키는 struct page 구조체를 반환, hash_entry는 hash_elem 포인터를 실제 struct page 포인터로 변환
}

/* PAGE를 spt에 삽입합니다. 삽입 시 유효성 검사를 수행합니다. */
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
	struct thread *curr = thread_current();
	struct list_elem *e = start;

	for (start = e; start != list_end(&frame_table); start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}
	for (start = list_begin(&frame_table); start != e; start = list_next(start))
	{
		victim = list_entry(start, struct frame, frame_elem);
		if (pml4_is_accessed(curr->pml4, victim->page->va))
			pml4_set_accessed(curr->pml4, victim->page->va, 0);
		else
			return victim;
	}
	return victim;
}

/* 하나의 페이지를 swap out 하고 해당 프레임을 반환합니다.
 * 실패 시 NULL을 반환합니다. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 swap out 하고 해당 프레임을 반환합니다. */
	swap_out(victim->page);
	return victim;
}

/* palloc()을 사용하여 프레임을 얻습니다.
 * 사용 가능한 페이지가 없다면, 프레임을 eviction 하여 메모리를 확보합니다.
 * 항상 유효한 주소를 반환해야 합니다. */
static struct frame *
vm_get_frame (void) {
	/* TODO: 이 함수를 구현하세요. */
	struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
	frame->kva=palloc_get_page(PAL_USER);

	if(frame->kva == NULL)
	{
		frame = vm_evict_frame();
		frame->page=NULL;
		return frame;
	}

	list_push_back(&frame_table, &frame->frame_elem);

	frame -> page = NULL;

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 성장시킵니다. */
static void
vm_stack_growth (void *addr UNUSED) {
	if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
	{
		vm_claim_page(addr);
		thread_current()->stack_bottom-=PGSIZE;
	}
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
	if(is_kernel_vaddr(addr)) return false;

	void *rsp_stack = is_kernel_vaddr(f->rsp) ? thread_current()->rsp_stack : f->rsp;
	if(not_present)
	{
		if(!vm_claim_page(addr))
		{
			if(rsp_stack - 8 <= addr && USER_STACK - 0x100000 <= addr && addr <= USER_STACK)
			{
				vm_stack_growth(thread_current()->stack_bottom-PGSIZE);
				return true;
			}
			return false;
		}
		else
			return true;
	}
	return false;
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
    page = spt_find_page(&thread_current()->spt,va);

    if(page == NULL){
        return false;
    }

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
	if(install_page(page->va, frame->kva, page->writable))
	{
        return swap_in(page, frame->kva);
    }
    return false;
}

void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	hash_init(&spt->pages, page_hash, page_less, NULL);
}

/* 보조 페이지 테이블을 src로부터 dst로 복사합니다. */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	// project 3
	struct hash_iterator i;
	hash_first(&i, &src->pages);
	while (hash_next(&i))
	{
		struct page *parent_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = page_get_type(parent_page);
		void *upage = parent_page->va;
		bool writable = parent_page->writable;
		vm_initializer *init = parent_page->uninit.init;
		void *aux = parent_page->uninit.aux;

		if (parent_page->operations->type != VM_UNINIT)
		{
			// dst에 페이지 만들고 즉시 클레임
			if (!vm_alloc_page(type, upage, writable))
				return false;
			if (!vm_claim_page(upage))
				return false;

			// dst에서 페이지 찾아서 프레임 내용 복사
			struct page *child_page = spt_find_page(dst, upage);
			if (child_page == NULL)
				return false;

			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
			continue;
		}
		else if (parent_page->operations->type == VM_UNINIT)
		{
			if (!vm_alloc_page_with_initializer(type, upage, writable, init, aux))
				return false;
		}
		else
		{
			if (!vm_alloc_page(type, upage, writable))
				return false;
			if (!vm_claim_page(upage))
				return false;
		}

		if (parent_page->operations->type != VM_UNINIT)
		{
			struct page *child_page = spt_find_page(dst, upage);
			memcpy(child_page->frame->kva, parent_page->frame->kva, PGSIZE);
		}
	}
	return true;
}

static void page_destructor(struct hash_elem *e, void *aux UNUSED)
{
	struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}
/* 보조 페이지 테이블이 사용하는 자원을 해제합니다. */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: 해당 스레드가 보유한 모든 supplemental_page_table을 제거하고,
	 * TODO: 수정된 내용을 저장소에 기록합니다. */
	hash_destroy(&spt->pages, page_destructor);
}
