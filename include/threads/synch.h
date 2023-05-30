#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore {
	unsigned value;             /* Current value. */
	struct list waiters;        /* List of waiting threads. */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock {
	struct thread *holder;      /* Thread holding lock (for debugging).락을 소유한 스레드 (디버깅용) */
	struct semaphore semaphore; /* Binary semaphore controlling access. 접근을 제어하는 이진 세마포어. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. 조건 변수 */
struct condition {
	struct list waiters;        /* List of waiting threads. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.

The compiler will not reorder operations across an
optimization barrier.  See "Optimization Barriers" in the
reference guide for more information.
최적화 배리어.

최적화 배리어를 통해 컴파일러는 연산들을 재배치하지 않습니다.
자세한 정보는 "Optimization Barriers"를 참조하십시오. */
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
