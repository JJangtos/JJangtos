/* file.c: 메모리에 매핑된 파일 객체(mmaped object)를 위한 구현입니다. */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* 이 구조체는 수정하지 마세요 */
static const struct page_operations file_ops = {
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

    /* file-backed는 aux(container)를 uninit.aux에 들고 있다가
       swap_in 시 그 정보를 사용합니다. (여기서는 추가 작업 없음) */
    struct file_page *file_page = &page->file;
    (void)file_page;
    return true;
}

/* 파일에서 내용을 읽어 페이지를 swap in 합니다. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;

        if(page == NULL){
        return false;
    }

    struct container *aux = (struct container*)page->uninit.aux;

    struct file *file = aux->file;
    off_t offset = aux->offset;
    size_t page_read_bytes = aux->page_read_bytes;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    file_seek(file, offset);

    if(file_read(file, kva, page_read_bytes) != (int)page_read_bytes){
        return false;
    }

    memset(kva + page_read_bytes, 0, page_zero_bytes);

    return true;
}

/* 페이지의 내용을 파일에 writeback 하여 swap out 합니다. */
static bool
file_backed_swap_out (struct page *page) {
    if (page == NULL) return false;

    struct container *aux = (struct container *)page->uninit.aux;

    /* dirty면 파일에 반영 (프레임 KVA에서 써야 함) */
    if (pml4_is_dirty(thread_current()->pml4, page->va)) {
        file_write_at(aux->file, page->frame->kva, aux->page_read_bytes, aux->offset);
        pml4_set_dirty(thread_current()->pml4, page->va, 0);
    }

    /* 매핑 해제 */
    pml4_clear_page(thread_current()->pml4, page->va);
    /* 프레임은 evict 호출자가 재사용하므로 여기서 NULL 처리만 */
    page->frame = NULL;
    return true;
}

/* 파일 기반 페이지를 제거합니다. PAGE는 호출자가 해제합니다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

// static bool lazy_load_file(struct page *page, void *aux) {
//     /* 최초 클레임 시 디스크에서 읽어오는 경로: aux를 직접 사용 */
//     struct container *cont = aux;
//     struct file *file = cont->file;
//     off_t offset = cont->offset;
//     size_t page_read_bytes = cont->page_read_bytes;

//     file_seek(file, offset);
//     if (file_read(file, page->frame->kva, page_read_bytes) != (int)page_read_bytes) {
//         return false;
//     }
//     memset(page->frame->kva + page_read_bytes, 0, PGSIZE - page_read_bytes);
//     return true;
// }


// /* mmap 작업을 수행합니다 */
// void *
// do_mmap(void *addr, size_t length, int writable,
// 		struct file *file, off_t offset)
// {
// 	struct file *mfile = file_reopen(file);

//     void *ori_addr = addr;
//     size_t read_bytes = length > file_length(file) ? file_length(file) : length;
//     size_t zero_bytes = PGSIZE - read_bytes % PGSIZE;

//     while(read_bytes > 0 || zero_bytes > 0){
//         size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
//         size_t page_zero_bytes = PGSIZE - page_read_bytes;

//         struct container *container = (struct container*)malloc(sizeof(struct container));
//         container->file = mfile;
//         container->offset = offset;
//         container->page_read_bytes = page_read_bytes;

//         if(!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_file, container)){
//             return NULL;
//         }
//         read_bytes -= page_read_bytes;
//         zero_bytes -= page_zero_bytes;
//         addr += PGSIZE;
//         offset += page_read_bytes;

//     }
//     return ori_addr;
// }

// void
// do_munmap (void *addr) {
//     while (true) {
//         struct page* page = spt_find_page(&thread_current()->spt, addr);
//         if (page == NULL || page_get_type(page) != VM_FILE)
//             break;

//         struct container *aux = (struct container *)page->uninit.aux;

//         /* dirty면 write-back (프레임 KVA 기준) */
//         if (pml4_is_dirty(thread_current()->pml4, page->va)) {
//             file_write_at(aux->file, page->frame->kva, aux->page_read_bytes, aux->offset);
//             pml4_set_dirty(thread_current()->pml4, page->va, 0);
//         }

//         /* 매핑 해제 */
//         pml4_clear_page(thread_current()->pml4, page->va);

//         /* 다음 페이지 */
//         addr += PGSIZE;
//     }

/* ========== 향상된 lazy_load_file ========== */
static bool lazy_load_file(struct page *page, void *aux) {
    struct container *cont = (struct container *)aux;
    
    // NULL 체크
    if (!cont || !cont->file || !page->frame || !page->frame->kva) {
        return false;
    }
    
    struct file *file = cont->file;
    off_t offset = cont->offset;
    size_t page_read_bytes = cont->page_read_bytes;
    
    // 파일 크기 체크
    if (offset >= file_length(file)) {
        // 파일 끝을 넘어선 경우 - 0으로 채움
        memset(page->frame->kva, 0, PGSIZE);
        // aux를 file.aux로 이동
        page->file.aux = aux;
        return true;
    }
    
    file_seek(file, offset);
    int bytes_read = file_read(file, page->frame->kva, page_read_bytes);
    
    if (bytes_read != (int)page_read_bytes) {
        return false;
    }
    
    // 나머지 부분을 0으로 채움
    memset(page->frame->kva + page_read_bytes, 0, PGSIZE - page_read_bytes);
    
    // 중요: aux를 file.aux로 이동 (페이지가 로드된 후)
    page->file.aux = aux;
    
    return true;
}

void *
do_mmap(void *addr, size_t length, int writable,
        struct file *file, off_t offset)
{
    if (file == NULL || length == 0)
        return NULL;
        
    struct file *mfile = file_reopen(file);
    if (mfile == NULL)
        return NULL;
        
    void *ori_addr = addr;
    size_t file_size = file_length(mfile);
    
    // 오프셋이 파일 크기를 초과하는지 확인
    if (offset >= file_size) {
        file_close(mfile);
        return NULL;
    }
    
    // 페이지 정렬 확인
    if (pg_ofs(addr) != 0) {
        file_close(mfile);
        return NULL;
    }
    
    size_t remaining_length = length;
    off_t current_offset = offset;
    void *current_addr = addr;
    
    while (remaining_length > 0) {
        // 이미 사용 중인 주소인지 확인
        if (spt_find_page(&thread_current()->spt, current_addr) != NULL) {
            do_munmap(ori_addr);
            file_close(mfile);
            return NULL;
        }
        
        // 이 페이지에서 읽을 바이트 수 계산
        size_t page_read_bytes;
        if (current_offset < file_size) {
            size_t available_in_file = file_size - current_offset;
            size_t needed_for_page = remaining_length < PGSIZE ? remaining_length : PGSIZE;
            page_read_bytes = available_in_file < needed_for_page ? available_in_file : needed_for_page;
        } else {
            page_read_bytes = 0;
        }
        
        struct container *container = (struct container*)malloc(sizeof(struct container));
        if (container == NULL) {
            do_munmap(ori_addr);
            file_close(mfile);
            return NULL;
        }
        
        container->file = mfile;
        container->offset = current_offset;
        container->page_read_bytes = page_read_bytes;
        
        if (!vm_alloc_page_with_initializer(VM_FILE, current_addr, writable, lazy_load_file, container)) {
            free(container);
            do_munmap(ori_addr);
            file_close(mfile);
            return NULL;
        }
        
        remaining_length -= PGSIZE;
        current_addr += PGSIZE;
        current_offset += page_read_bytes;
    }
    
    return ori_addr;
}

void
do_munmap (void *addr) {
    struct thread *curr = thread_current();
    struct file *file_to_close = NULL;
    
    while (true) {
        struct page* page = spt_find_page(&curr->spt, addr);
        if (page == NULL)
            break;
            
        // VM_FILE 타입인 경우만 처리
        if (page_get_type(page) != VM_FILE) {
            addr += PGSIZE;
            continue;
        }
        
        struct container *aux = NULL;
        
        // 페이지 상태에 따라 올바른 aux 정보 가져오기
        // uninit_ops는 static이므로 직접 비교할 수 없음
        // 대신 페이지가 초기화되었는지 frame 존재 여부로 판단
        if (page->frame != NULL) {
            // 이미 로드된 페이지 - file.aux 사용
            aux = (struct container *)page->file.aux;
        } else {
            // 아직 로드되지 않은 페이지 - uninit.aux 사용
            aux = (struct container *)page->uninit.aux;
        }
        
        // write-back 처리 (dirty한 경우만)
        if (aux && aux->file) {
            if (!file_to_close) {
                file_to_close = aux->file; // 첫 번째 파일 참조만 저장
            }
            
            // 페이지가 실제로 메모리에 로드되어 있고 dirty한 경우
            if (page->frame && page->frame->kva && pml4_is_dirty(curr->pml4, page->va)) {
                file_write_at(aux->file, page->frame->kva, aux->page_read_bytes, aux->offset);
                pml4_set_dirty(curr->pml4, page->va, 0);
            }
        }
        
        // aux 구조체 해제
        if (aux) {
            free(aux);
        }
        
        // 물리 프레임 해제 (할당되어 있는 경우)
        if (page->frame) {
            palloc_free_page(page->frame->kva);
            free(page->frame);
        }
        
        // 페이지 테이블에서 매핑 제거
        pml4_clear_page(curr->pml4, page->va);
        
        // SPT에서 페이지 제거
        hash_delete(&curr->spt.pages, &page->hash_elem);
        
        // 페이지 구조체 해제
        free(page);
        
        addr += PGSIZE;
    }
    
    // 파일 닫기 (마지막에 한 번만)
    if (file_to_close) {
        file_close(file_to_close);
    }
}

