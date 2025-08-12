/* file.c: 메모리에 매핑된 파일 객체(mmaped object)를 위한 구현입니다. */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* 이 구조체는 수정하지 마세요 */
const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* 파일 기반 가상 메모리 초기화 함수 */
void
vm_file_init (void) {
}

/* 파일 기반 페이지를 초기화합니다 */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 핸들러를 설정합니다 */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* 파일에서 내용을 읽어 페이지를 swap in 합니다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 페이지의 내용을 파일에 writeback 하여 swap out 합니다. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* 파일 기반 페이지를 제거합니다. PAGE는 호출자가 해제합니다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

static bool lazy_load_file(struct page *page, void *aux) {
    struct container *cont = aux;
    struct file *file = cont->file;
    off_t offset = cont->offset;
    size_t page_read_bytes = cont->page_read_bytes;

    file_seek(file, offset);
    if (file_read(file, page->frame->kva, page_read_bytes) != (int)page_read_bytes) {
        return false;
    }
    memset(page->frame->kva + page_read_bytes, 0, PGSIZE - page_read_bytes);
    return true;
}


/* mmap 작업을 수행합니다 */
void *
do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset)
{
	struct file *mfile = file_reopen(file);

    void *ori_addr = addr;
    size_t read_bytes = length > file_length(file) ? file_length(file) : length;
    size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

    while(read_bytes > 0 || zero_bytes > 0){
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct container *container = (struct container*)malloc(sizeof(struct container));
        container->file = mfile;
        container->offset = offset;
        container->page_read_bytes = page_read_bytes;

        if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, container)){
            return NULL;
        }
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        addr += PGSIZE;
        offset += page_read_bytes;

    }
    return ori_addr;
}

void
do_munmap (void *addr) {
	while(true){
        struct page* page = spt_find_page(&thread_current()->spt, addr);

        if(page == NULL)
            break;

        struct container *aux = (struct container *)page->uninit.aux;

        // dirty check
        if(pml4_is_dirty(thread_current()->pml4, page->va)){
            file_write_at(aux->file, addr, aux->page_read_bytes, aux->offset);
            pml4_set_dirty(thread_current()->pml4, page->va, 0);
        }

        pml4_clear_page(thread_current()->pml4, page->va);
        addr += PGSIZE;
    }
}