/* anon.c: 디스크 이미지가 아닌 페이지(즉, anonymous page)를 위한 구현. */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "kernel/bitmap.h"

/* 아래 줄은 수정하지 마세요 */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* 이 구조체는 수정하지 마세요 */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

struct bitmap *swap_table;
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

/* Anonymous 페이지를 위한 데이터를 초기화합니다 */
void
vm_anon_init (void) {
	/* TODO: swap_disk를 설정하세요. */
	swap_disk = disk_get(1,1);
    size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
    swap_table = bitmap_create(swap_size);
}

/* 파일 매핑을 초기화합니다 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
    /* 핸들러를 설정합니다 */
    page->operations = &anon_ops;

    /* 익명 페이지 메타 초기화 */
    struct anon_page *anon_page = &page->anon;
    anon_page->swap_index = -1;
    return true;
}

/* swap 디스크에서 내용을 읽어 페이지를 swap in 합니다. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	    // swap out된 page가 disk swap영역 어느 위치에 저장되었는지는 
    // anon_page 구조체 안에 저장되어 있다.
    int page_no = anon_page->swap_index;

    if(bitmap_test(swap_table, page_no) == false){
        return false;
    }
    // 해당 swap 영역의 data를 가상 주소공간 kva에 써준다.
    for(int i=0; i< SECTORS_PER_PAGE; ++i){
        disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
    }
    // 해당 swap slot false로 만들어줌(다음번에 쓸 수 있게)
    bitmap_set(swap_table, page_no, false);

    return true;
}

/* 페이지의 내용을 swap 디스크에 써서 swap out 합니다. */
static bool
anon_swap_out (struct page *page) {
    struct anon_page *anon_page = &page->anon;

    int page_no = bitmap_scan(swap_table, 0, 1, false);
    if (page_no == BITMAP_ERROR) {
        return false;
    }

    /* 반드시 프레임 KVA로부터 디스크에 써야 함 */
    for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
        disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i,
                   page->frame->kva + DISK_SECTOR_SIZE * i);
    }

    bitmap_set(swap_table, page_no, true);
    pml4_clear_page(thread_current()->pml4, page->va);
    anon_page->swap_index = page_no;

    /* 프레임과의 연결은 끊어줌 (evict 경로에서 재사용) */
    page->frame = NULL;
    return true;
}

/* anonymous 페이지를 파괴합니다. PAGE는 호출자가 해제합니다. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
