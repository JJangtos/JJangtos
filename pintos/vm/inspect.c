/* inspect.c: 가상 메모리(VM) 테스트 유틸리티 */
/* 이 파일은 수정하지 마세요. */

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "vm/inspect.h"

static void
inspect (struct intr_frame *f) {
	const void *va = (const void *) f->R.rax;
	f->R.rax = PTE_ADDR (pml4_get_page (thread_current ()->pml4, va));
}

/* 가상 메모리 컴포넌트를 테스트하기 위한 도구입니다.
 * int 0x42 인터럽트를 통해 이 함수를 호출할 수 있습니다.
 * 입력:
 *   @RAX - 조사할 가상 주소(Virtual address)
 * 출력:
 *   @RAX - 입력된 가상 주소에 매핑된 물리 주소(Physical address) */
void
register_inspect_intr (void) {
	intr_register_int (0x42, 3, INTR_OFF, inspect, "Inspect Virtual Memory");
}
