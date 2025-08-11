#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	struct file *file;   /* Reopened handle owned by this page */
    off_t offset;        /* File offset for this page */
    size_t read_bytes;   /* Bytes to read from file into this page */
    bool is_last;        /* Marks the last page in this mapping */
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
#endif
