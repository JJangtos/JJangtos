#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
static struct thread * get_child_process(tid_t child_tid);

struct dict_elem {
    struct file *key;    // 부모의 원본
    struct file *value;  // 자식의 복사본
};

// aux 구조체 생성
struct aux_info {
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
};

#endif /* userprog/process.h */
