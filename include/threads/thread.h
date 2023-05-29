#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle.
스레드 라이프 사이클 */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. 실행 중인 스레드 */
	THREAD_READY,       /* Not running but ready to run. 실행 대기 중인 스레드*/
	THREAD_BLOCKED,     /* Waiting for an event to trigger. 조건을 기다리는 스레드*/
	THREAD_DYING        /* About to be destroyed. 파괴될 예정인 스레드*/
};

/* Thread identifier type.
   You can redefine this to whatever type you like. 
   스레드 식별자 타입
   원하는 대로 재정의 가능*/
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. tid_t의 오류값 */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. 가장 낮은 우선순위*/
#define PRI_DEFAULT 31                  /* Default priority. 기본 우선순위*/
#define PRI_MAX 63                      /* Highest priority. 가장 높은 우선순위*/

/** A kernel thread or user process.
 * 커널 스레드 또는 사용자 프로세스
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 * 각 스레드 구조체는 고유한 4 kB 페이지에 저장됩니다.
 * 스레드 구조체 자체는 페이지의 맨 아래에 위치합니다.
 * (오프셋 0에서 시작합니다).
 * 나머지 페이지는 스레드의 커널 스택에 예약되어 있으며,
 * 페이지의 맨 위에서부터 아래 방향으로 확장됩니다.
 * 다음은 그림으로 표현한 것입니다:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 The upshot of this is twofold:
 이로 인해 두 가지 결과가 나타난다.

    1. First, `struct thread' must not be allowed to grow too
       big.  If it does, then there will not be enough room for
       the kernel stack.  Our base `struct thread' is only a
       few bytes in size.  It probably should stay well under 1
       kB.
	1. 첫째로, `struct thread'가 너무 커지는 것을 허용해서는 안됩니다.
		그렇게 되면 커널 스택에 충분한 공간이 없게 됩니다. 
		우리의 기본 `struct thread'는 몇 바이트 크기입니다. 
		보통 1KB 이하로 유지되어야 합니다.

    2. Second, kernel stacks must not be allowed to grow too
       large.  If a stack overflows, it will corrupt the thread
       state.  Thus, kernel functions should not allocate large
       structures or arrays as non-static local variables.  Use
       dynamic allocation with malloc() or palloc_get_page()
       instead.
	2. 둘째로, 커널 스택이 너무 커지지 않도록 해야 합니다.
   		스택이 오버플로우되면 스레드 상태가 손상될 수 있습니다.
   		따라서 커널 함수에서는 큰 구조체나 배열을 정적 로컬 변수로 할당해서는 안됩니다.
   		대신 malloc()이나 palloc_get_page()를 사용하여 동적 할당을 해야 합니다.

	The first symptom of either of these problems will probably be
	an assertion failure in thread_current(), which checks that
	the `magic' member of the running thread's `struct thread' is
	set to THREAD_MAGIC.  Stack overflow will normally change this
	value, triggering the assertion. 
	이러한 문제 중 하나의 첫 번째 증상은 보통 thread_current()에서 단언문 실패가 발생할 것입니다.
	이 단언문은 실행 중인 스레드의 struct thread의 magic 멤버가 THREAD_MAGIC로 설정되었는지 확인합니다.
	스택 오버플로우는 일반적으로 이 값을 변경하여 단언문을 트리거합니다.*/

	/* The `elem' member has a dual purpose.  It can be an element in
	the run queue (thread.c), or it can be an element in a
	semaphore wait list (synch.c).  It can be used these two ways
	only because they are mutually exclusive: only a thread in the
	ready state is on the run queue, whereas only a thread in the
	blocked state is on a semaphore wait list.
	
	elem 멤버는 이중 목적을 가지고 있습니다.
	이는 실행 대기열(run queue)에서의 요소이거나 
	세마포어 대기 목록(semaphore wait list)에서의 요소가 될 수 있습니다. 
	이 두 가지 방식으로만 사용될 수 있는 이유는 서로 배타적인(mutually exclusive)이기 때문입니다:
	실행 대기 상태인 스레드만 실행 대기열에 있으며, 차단(blocked) 상태인 스레드만 세마포어 대기 목록에 있습니다.
	*/

struct thread {
	/* Owned by thread.c.  thread.c에서 소유합니다*/
	tid_t tid;                          /* Thread identifier.스레드 식별자(thread identifier).  */
	enum thread_status status;          /* Thread state. 스레드 상태(thread state).*/
	char name[16];                      /* Name (for debugging purposes).이름 (디버깅 용도) */
	int priority;                       /* Priority. 우선순위. */

	/* Shared between thread.c and synch.c. 
	thread.c와 synch.c 사이에서 공유됩니다.*/
	struct list_elem elem;              /* List element. 목록 요소*/
	int64_t wakeup_tick;				//!프로젝트.1 일어나야 할 tick
#ifdef USERPROG
	/* Owned by userprog/process.c. userprog/process.c에서 소유합니다. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread.
	스레드가 소유한 전체 가상 메모리에 대한 테이블입니다.*/
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. thread.c에서 소유합니다. */
	struct intr_frame tf;               /* Information for switching 전환 정보 */
	unsigned magic;                     /* Detects stack overflow.  스택 오버플로우 감지*/
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs".
   false (기본값)이면 round-robin 스케줄러를 사용합니다.
   true이면 다중 피드백 큐 스케줄러를 사용합니다.
   커널 커맨드 라인 옵션 "-o mlfqs"로 제어됩니다. */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_sleep(int64_t ticks);							//!실행 중인 스레드를 슬립으로 재운다.
void thread_awake(int64_t ticks);							//!슬립 큐의 스레드를 깨운다.
void update_next_tick_to_awake(int64_t ticks);				//!최소 틱을 가진 스레드 저장
int64_t get_next_tick_to_awake(void);						//!thread.c의 next_tick_to_awake 반환



void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

#endif /* threads/thread.h */
