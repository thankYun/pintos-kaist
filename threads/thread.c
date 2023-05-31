#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/*
   Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. 
   
   struct thread 스레드를 나타내는 자료 구조, magic 멤퍼는 스택 오버플로우를 감지하는 보조 용도로 사용.
   스택 오버플로우가 발생하면 magic 멤버의 값이 변경되어 스택의 무결성을 확인할 수 있다.
   */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* 준비된 스레드의 목록, 실행 준비가 되었지만 현재 실행되고 있지 않은 스레드의 목록 */
static struct list ready_list;

/* Idle thread. 유휴 스레드 */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main().
   초기 스레드, 이 스레드는 init.c:main()에 있습니다. */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests
	스레드 파괴 요청 */
static struct list destruction_req;

/* Statistics. 통계 자료 */
static long long idle_ticks;    /* # of timer ticks spent idle. 유휴 상태로 소모된 타이머 틱 */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. 커널 스레드의 타이머 틱*/
static long long user_ticks;    /* # of timer ticks in user programs. 유저 프로그램의 타이머 틱*/

//! 프로젝트 1.1
static struct list sleep_list;				//!스레드 블록 상태의 스레드를 관리하기 위한 리스트 자료구조
static int64_t next_tick_to_awake;			//!슬립 리스트에서 대기 중인 스레드들의 wakeup_tick값 중 최솟값을 저장
static long long next_tick_to_awake;


/* Scheduling. 스케쥴링*/
#define TIME_SLICE 4            /* # of timer ticks to give each thread. 스레드에 대한 타이머 틱 수 정의 */
static unsigned thread_ticks;   /* # of timer ticks since last yield. 마지막 양보 이후 타이머 틱 수 정의 */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs".

   FALSE인 경우 round-robin 스케쥴러 방식 사용
   TRUE인 경우 다단계 피드백 큐 스케쥴러 방식 사용
   이 설정은 커널의 명령줄 옵션 "-o mlfqs"로 제어됨
   Round-robin 스케줄러: 모든 스레드가 동일한 우선순위를 가지고 CPU 시간을 순환하며 할당하는 방식
   각 스레드는 동일한 실행 시간을 가지고 순서대로 CPU를 사용
   multi-level feedback queue scheduler: 스레드를 다양한 우선순위로 분류하고 우선순위에 따라 다른 스케쥴링
   적용, 스레드의 실행 시간, 우선순위, CPU 사용량 등의 요소를 기반으로 스레드를 적절한 우선순위 레벨로 이동
   시스템 성능을 향상시킬 수 있음
   -o mlfqs커널 명령줄 옵션으로 다단계 피드백 큐 스케쥴러를 활성화하거나 비활성화 할 수 있다.*/
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Returns true if T appears to point to a valid thread. 
T가 유효한 스레드를 가리키는 것 같으면 true 반환 */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/** Returns the running thread.
 * 실행 중인 스레드 반환 과정
 * Read the CPU's stack pointer `rsp', and then round that
 * CPU의 스택 포인터인 'rsp'읽고 반올림
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. 
 * 페이지 시작점까지 CPU의 스택 포인터를 내려 현재 스레드를 찾는다.
 * */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.	스레드 스타트에 대한 전역 설명자 테이블이다.
// Because the gdt will be setup after the thread_init, we should setup temporal gdt first.
// gdt가 thread_init 뒤에 설정되기 때문에 임시 gdt를 생성해야 한다
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes.
   
   현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화한다.
   이는 일반적으로 작동하지 않을 수 있으나, 이 경우에 가능한 것은 
   loader.S가 스택의 맨 아래를 페이지 경계에 위치시키도록 신경 쓰기 때문
   또한 실행 대기열(run queue)과 tid 락을 초기화합니다.
   이 함수를 호출한 후에는 thread_create()를 사용하여
   스레드를 생성하기 전에 페이지 할당기를 초기화해야 합니다.
   이 함수가 완료되기 전까지 thread_current()를 호출하는 것은 안전하지 않습니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	사용자 컨텍스트가 없는 임시 gdt를 다시 로드
	This gdt does not include the user context.
	The kernel will rebuild the gdt with user context, in gdt_init ().
	커널은 사용자 컨텍스트를 포함하여 gdt_init에서 gdt를 다시 구축 */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context 전역 스레드 컨텍스트 */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);
//! 프로젝트 1.1
	list_init (&sleep_list);				//!슬립 큐 & next tick to awake 초기화 코드 추가
	// next_tick_to_awake = INT64_MAX;			//!최솟값 찾아가야 하니 초기화할 때는 정수 최댓값

	/* Set up a thread structure for the running thread. 실행 중인 스레드에 대한 구조를 설정*/
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread.
   인터럽트를 활성화하여 선제적 스레드 스케줄링 시작, 유휴 스레드 생성 */
void
thread_start (void) {
	/* Create the idle thread. 유휴 스레드 생성 */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. 선제적 스레드스케줄링 시작 */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. 유휴 스레드가 idle_thread를 초기화 할 때까지 대기 */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. 
   각 타이머 틱마다 타이머 인터럽트 핸들러에 의해 호출됩니다.
   따라서 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다.*/
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. 통계 업데이트*/
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. 선제 시작*/
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3.
   
   주어진 초기 우선순위(PRIORITY)로 FUNCTION을 실행하고 인자로 AUX를 전달하는 NAME이라는 새로운 커널 스레드를 생성하고 준비 큐(ready queue)에 추가합니다.
   새로운 스레드의 스레드 식별자(TID)를 반환하며, 생성에 실패한 경우 TID_ERROR를 반환합니다.
   만약 thread_start()가 호출되었다면, 새로운 스레드는 thread_create()가 반환되기 전에 스케줄될 수 있습니다. 심지어 thread_create()가 반환되기 전에 종료될 수도 있습니다.
   반대로, 새로운 스레드가 스케줄되기 전에 원래의 스레드가 임의의 시간 동안 실행될 수 있습니다. 순서를 보장해야 한다면 세마포어나 다른 형태의 동기화를 사용하세요.
   제공된 코드는 새로운 스레드의 'priority' 멤버를 PRIORITY로 설정하지만, 실제 우선순위 스케줄링은 구현되지 않았습니다. 우선순위 스케줄링은 문제 1-3의 목표입니다. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	struct thread *curr = thread_current(); //! 1.2 - 실행중인 스레드와 비교하기 위해 curr 선언
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. 스레드 할당 */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. 스레드 초기화*/
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/** Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument.
	 * 스케쥴된 경우 kernel_thread 호출
	 * rdi는 첫 번째 인수, rsi는 두번째 인수를 의미한다.
	 */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. ready queue에 줄 세우기 위함 */
	thread_unblock (t);
	// //! 1.2
	if( cmp_priority(&t->elem, &curr->elem, NULL)){							//실행중인 스레드와 우선순위 비교 후 생성된 스레드 우선순위가 높으면 양보
		thread_yield();
	}
	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. 스레드를 일시정지한다. thread_unblock()에 의해 깨울 때까지 다시 예약되지 않음 
   인터럽트를 끈 상태에서 이 기능을 호출해야 한다. 일반적으로 동기화 프리미티브 중 하나를 synch.h에 사용하는 것이 좋다. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data.
   차단된 스레드 T를 실행 준비 상태로 전환
   T가 차단되지 않은 경우 오류(실행 중인 스레드를 준비 상태로 만들기 위해 thread_yield()를 사용할 것)
   이 함수는 실행 중인 스레드를 선점하지 않습니다. 이는 중요할 수 있다. 
   호출자가 인터럽트를 직접 비활성화한 경우, 스레드를 원자적으로 블로킹 해제하고 다른 데이터를 업데이트할 수 있다고 기대할 수 있다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();		//인터럽트 종료
	ASSERT (t->status == THREAD_BLOCKED);
	//! 프로젝트 1.2
	//! list_push_back (&ready_list, &t->elem); 맨뒤로 들어가지 않는다
	list_insert_ordered(&ready_list, &t->elem, &cmp_priority, NULL);

	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. 실행 중인 스레드 이름 반환 */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details.
   현재 실행 중인 스레드를 반환합니다.
   이는 running_thread()에 몇 가지 안전성 확인을 추가한 것입니다.
   자세한 내용은 thread.h 파일 상단의 큰 주석을 참조하세요. */

struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* T가 실제로 스레드인지 확인합니다.
	만약 이러한 단언문 중 하나가 실행되면, 해당 스레드의 스택이 오버플로우된 것일 수 있습니다.
	 각 스레드는 4 kB 미만의 스택을 가지므로, 큰 자동 배열 몇 개나 중간 정도의 재귀 호출은 스택 오버플로우를 일으킬 수 있습니다. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. 실행 중인 스레드의 tid 반환 */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller.
   현재 스레드의 스케쥴을 절대적으로 취소하고 삭제한다.
   호출자에게 리턴 */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail().
	   상태를  "dying"으로 설정하고 다른 프로세스를 스케줄, 이들은 schedule_tail() 함수 중에 파괴될 예정 */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim.
   CPU를 반환합니다. 현재 스레드는 sleep 상태가 아니며
   스케줄러의 요청에 따라 즉시 다시 예약할 수 있습니다. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ()); //외부 인터럽트 프로세스 수행 중이면 TRUE, 아니면 FALSE

	old_level = intr_disable ();	// 인터럽트 비활성화하고 old_level 받아옴
	if (curr != idle_thread)		// 현재 스레드가 유휴 상태가 아니면
		list_insert_ordered(&ready_list, &curr->elem, &cmp_priority,NULL); //!우선순위대로 ready queue에 배정함
		//! list_push_back (&ready_list, &curr->elem);	
	do_schedule (THREAD_READY);		//context switch 수행, 진행중인 스레드를 ready로
	intr_set_level (old_level);		//인자로 전달된 인터럽트 상태로 인터럽트를 성정하고 이전 인터럽트 상태를 변환한다
}
//! 프로젝트1.1 > yield 기반으로 제작
void thread_sleep(int64_t ticks){
	struct thread *curr =thread_current();
	enum intr_level old_level;

	//! 스레드를 blocked 상태로 만들고 슬립 큐에 삽입해서 대기
	ASSERT (!intr_context()); //!외부 인터럽트 프로세스 수행 중이면 TRUE, 아니면 FALSE

	old_level =intr_disable();

	curr -> wakeup_tick = ticks;	//! 현재 스레드를 깨우는 tick은 인자로 받은 tick만큼의 시간이 지나고서 작동
	
	//! 스레드 슬립 큐에 삽입
	if (curr != idle_thread){
		list_push_back (&sleep_list, &curr ->elem);
	}
	
	update_next_tick_to_awake(ticks); //! 다음 깨워야 할 스레드가 바뀔 가능성이 있으니 최소 틱 저장
	do_schedule(THREAD_BLOCKED);	//! context switch: blocked(sleep) 상태로 바꿔줌
	intr_set_level (old_level);	
}


/* Sets the current thread's priority to NEW_PRIORITY.
현재 스레드의 우선 순위를 NEW_PRIORITY로 설정합니다 */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
	//! 1.2
	/**준비 리스트 앞에서 기다리고 있던 스레드와 우선순위를 비교
	 * ready 리스트에 기다리고 있던 스레드의 우선순위가 더 높다면 CPU를 기다리고 있던 스레드에 실행 양보. 
	*/
	test_max_priority();	}

/* Returns the current thread's priority.
현재 스레드의 우선 순위를 반환합니다. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE.
현재 스레드의 nice 값을 NICE로 설정합니다. */
//todo
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. 현재 스레드의 nice 값을 반환합니다. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. 시스템 로드 평균의 100배를 반환합니다. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. 현재 스레드의 recent_cpu 값의 100배를 반환합니다. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty.

   Idle 스레드입니다. 다른 실행 가능한 스레드가 없을 때 실행됩니다.
   Idle 스레드는 처음에 thread_start()에 의해 준비 리스트에 넣어집니다. 
   초기에 한 번 스케줄되며, 그 때 idle_thread를 초기화하고 전달된 세마포어를 "up"하여 thread_start()가 계속 진행되도록 하고 즉시 블록됩니다. 
   이후로는 idle 스레드가 준비 리스트에 나타나지 않습니다. 준비 리스트가 비어있을 때 특수한 경우로서 next_thread_to_run()에 의해 반환됩니다.*/
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction".

		   인터럽트를 다시 활성화하고 다음 인터럽트를 기다립니다.
		   "sti" 명령은 다음 명령의 완료까지 인터럽트를 비활성화합니다.
		   따라서 이 두 개의 명령은 원자적으로 실행됩니다.
		   이 원자성은 중요합니다. 그렇지 않으면, 인터럽트가 인터럽트를 다시 활성화하고 다음 인터럽트를 기다리는 사이에 처리될 수 있으며,
		   이로 인해 한 클록 틱에 해당하는 시간이 낭비될 수 있습니다.
		   [IA32-v2a] "HLT", [IA32-v2b] "STI", 및 [IA32-v3a] 7.11.1 "HLT Instruction"을 참조하세요. */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. 커널 스레드의 기반으로 사용되는 함수 */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. 스레드가 인터럽트를 끈 상태에서 실행 */
	function (aux);       /* Execute the thread function. 스레드 기능 실행 */
	thread_exit ();       /* If function() returns, kill the thread. 함수가 반환되면 스레드 종료 */
}


/* Does basic initialization of T as a blocked thread named NAME.
차단된 스레드(NAME)로 T를 초기화합니다 */
//todo 분석
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;

	//!프로젝트 1.2 donation
	t -> init_priority = priority;
	t -> wait_on_lock = NULL;
	list_init(&t->donations);
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. 
   예약할 다음 스레드를 선택하고 반환합니다. 
   실행 대기열이 비어 있지 않은 경우 실행 대기열에서 스레드를 반환해야 합니다.
   (실행 중인 스레드가 계속 실행될 수 있으면 실행 대기열에 있게 됩니다.) 
   실행 대기열이 비어 있으면 idle_thread를 반환합니다.*/
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread iretq를 사용하여 스레드 실행 */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. 
   새로운 스레드의 페이지 테이블을 활성화하여 스레드를 전환하고, 이전 스레드가 dying 상태인 경우 파괴합니다.
   이 함수가 호출될 때, 우리는 이미 PREV 스레드에서 전환한 상태이며, 새로운 스레드가 이미 실행 중이며 인터럽트가 아직 비활성화된 상태입니다.
   스레드 전환이 완료되기 전까지 printf()를 호출하는 것은 안전하지 않습니다. 실제로는 printf()를 함수의 끝에 추가해야 합니다.*/

static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 We first restore the whole execution context into the intr_frame
	 and then switching to the next thread by calling do_iret.
	 Note that, we SHOULD NOT use any stack from here
	 until switching is done. 
	 
	 주요한 전환 로직입니다. 우리는 먼저 전체 실행 컨텍스트를 intr_frame으로 복원한 다음 do_iret을 호출하여 다음 스레드로 전환합니다.
	 전환이 완료될 때까지 여기서는 어떠한 스택도 사용해서는 안 됩니다.*/
	__asm __volatile (
			/* Store registers that will be used. 사용될 레지스터 저장 */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip 최근 rip 읽기.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 This function modify current thread's status to status and then
 finds another thread to run and switches to it.
 It's not safe to call printf() in the schedule().
 
 
 새로운 프로세스를 스케줄합니다. 진입할 때 인터럽트는 꺼져 있어야 합니다.
 이 함수는 현재 스레드의 상태를 status로 변경한 다음 실행할 다른 스레드를 찾아 전환합니다.
 schedule()에서 printf()를 호출하는 것은 안전하지 않습니다. */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. 새 주소공간 활성화 */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule().
		   
		   만약 전환된 스레드가 dying 상태라면, 해당 스레드의 struct thread를 파괴합니다. 
		   이 작업은 thread_exit()가 스스로 바닥을 빼지 않도록 늦게 발생해야 합니다.
		   여기서는 페이지가 현재 스택에 사용되고 있기 때문에 페이지 해제 요청만 큐에 넣습니다.
		   실제 파괴 로직은 schedule()의 시작 부분에서 호출될 것입니다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information of current running. 
		 스레드를 전환하기 전에 먼저 현재 실행 중인 정보를 저장합니다
		 */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. 새 스레드에 사용할 tid를 반환 */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

void thread_awake(int64_t ticks) {
	/* Sleep queue에서 깨워야 할 thread를 찾아서 wakeup */
	next_tick_to_awake = INT64_MAX;
	struct list_elem *e = list_begin(&sleep_list); // sleep list의 첫번째 요소
	struct thread *t;

	/*  sleep_list의 스레드를 에서 wakeup_tick이 ticks보다 작으면 깨우기*/
	for (e; e != list_end(&sleep_list);)
	{
		t = list_entry(e, struct thread, elem); // sleep list에 들어있는 엔트리 중에서 계속 돌아
		if (t->wakeup_tick <= ticks) // wakeup_tick보다 지금 시간(ticks)이 크다면
		// 이때 지금 시간(ticks)은 thread_awake()를 실행하기 직전의 next_tick_to_awake.
		{
			e = list_remove(&t->elem); // 현재 t는 sleep_list에 연결된 스레드.
			thread_unblock(t); // 리스트에서 지우고 난 다음 unblock 상태로 변경!
			// 여기서는 list_remove에서 이미 다음 리스트로 연결해주니 list_next가 필요 X.
		}
		/* 위에서 thread가 일어났다면 sleep_list가 변했으니 next_tick_to_awake가 바뀜.
		현재 ticks에서 일어나지 않은 스레드 중 가장 작은 wakeup_tick이 next_tick_to_awake. 얘를 갱신. 
		즉, 아직 깰 시간이 안 된 애들 중 가장 먼저 일어나야 하는 애를 next_tick_to_awake!*/
		else {
			update_next_tick_to_awake(t->wakeup_tick);
			e= list_next(e); //위에서는 바로 리스트에서 지우니까 for문 돌고 나면 알아서 갱신되었으나 여기는 list_next로 갱신.
		}
	}
}

void update_next_tick_to_awake(int64_t ticks) {
	next_tick_to_awake = MIN(next_tick_to_awake,ticks);
}

int64_t get_next_tick_to_awake(void){
	return next_tick_to_awake;
}

//! 프로젝트 1.2 에서 생성된 함수
/**
 * 두 개 element가 주어지면 해당 요소의 스레드가 가진 우선순위를 비교하여 a가 b보다 높으면 1을 반환하도록 한다.
 * running 중인 스레드의 우선순위를 b로 주고 새로 시도하는 스레드를 a로 주자.
*/
bool cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
	struct thread *t_a; 
	struct thread *t_b;
	t_a = list_entry(a, struct thread, elem);
	t_b = list_entry(b, struct thread, elem);
	return ((t_a -> priority) > (t_b -> priority)) ? true : false;
}

//! 프로젝트 1.2 에서 생성된 함수
/**
 * 레디 리스트의 가장 앞에 있는(우선순위가 높은) 스레드와 비교해 양보한다.
*/
void test_max_priority(){
	if (list_empty(&ready_list)){
		return;
	}

	int run_priority = thread_current() -> priority;
	struct list_elem *e = list_begin(&ready_list);
	struct thread *t = list_entry(e,struct thread, elem);

	if (t-> priority > run_priority){
		thread_yield();
	}
}

//! 1.2 donation
/**
 * !우선순위를 lock에 연결된 모든 스레드에 빌려주기에
 * !wait_on_lock에서 기다리고 있는 lock를 현재 점유하고 있는 holder를 순회하면서
 * !모두에게 기부한다
*/
void donate_priority(void){
	int depth;
    struct thread *cur = thread_current();
    
    for (depth =0; depth < 8; depth++) {
    	if (!cur->wait_on_lock) 		// 더이상 연결된 lock이 없다면 종료
        	break;
        struct thread *holder = cur->wait_on_lock->holder;
        holder->priority = cur->priority;
        cur = holder;
    }
}

//! 프로젝트 1.2 donate에서 생성된 함수
/**
 * lock_release를 통해 lock이 사라졌으니 donation리스트에서 본인을 없앰
*/
void remove_with_lock (struct lock *lock)
{
  struct list_elem *e;
  struct thread *cur = thread_current ();

  for (e = list_begin (&cur->donations); e != list_end (&cur->donations); e = list_next (e)){
    struct thread *t = list_entry (e, struct thread, donation_elem);
    if (t->wait_on_lock == lock)
      list_remove (&t->donation_elem);				
  }
}

//! 프로젝트 1.2 donate에서 생성된 함수
/**
 * 도네이션 리스트 우선순위 재설정
*/
void refresh_priority (void)
{
  struct thread *cur = thread_current ();

  cur->priority = cur->init_priority;
  
  if (!list_empty (&cur->donations)) {					
    list_sort (&cur->donations, thread_compare_donate_priority, 0);

    struct thread *front = list_entry (list_front (&cur->donations), struct thread, donation_elem);
    if (front->priority > cur->priority)
      cur->priority = front->priority;
  }
}

//! 프로젝트 1.2 donate에서 생성된 함수
bool
thread_compare_donate_priority (const struct list_elem *l, 
				const struct list_elem *s, void *aux UNUSED)
{
	return list_entry (l, struct thread, donation_elem)->priority
		 > list_entry (s, struct thread, donation_elem)->priority;
}