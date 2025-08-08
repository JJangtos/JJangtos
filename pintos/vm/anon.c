/* anon.c: 디스크 이미지가 아닌 페이지(즉, anonymous page)를 위한 구현. */

#include "vm/vm.h"
#include "devices/disk.h"

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

/* Anonymous 페이지를 위한 데이터를 초기화합니다 */
void
vm_anon_init (void) {
	/* TODO: swap_disk를 설정하세요. */
	swap_disk = NULL;
}

/* 파일 매핑을 초기화합니다 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러를 설정합니다 */
	struct uinit_page *uninit = &page->uninit;
	memset(uninit, 0, sizeof(struct uninit_page));
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->swap_index=-1;

	return true;
}

/* swap 디스크에서 내용을 읽어 페이지를 swap in 합니다. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* 페이지의 내용을 swap 디스크에 써서 swap out 합니다. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* anonymous 페이지를 파괴합니다. PAGE는 호출자가 해제합니다. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
