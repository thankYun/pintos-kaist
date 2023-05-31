/*	 This file is derived from source code for the Nachos
  	 instructional operating system.  The Nachos copyright notice
   	is reproduced in full below. 
  	 이 파일은 Nachos의 소스 코드에서 파생되었습니다
	교육 운영 체제. 나초스 저작권 고시
	는 아래에 전체적으로 재현되어 있습니다.*/

	/* Copyright (c) 1992-1996 The Regents of the University of California.
	All rights reserved.

	Permission to use, copy, modify, and distribute this software
	and its documentation for any purpose, without fee, and
	without written agreement is hereby granted, provided that the
	above copyright notice and the following two paragraphs appear
	in all copies of this software.

	IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
	ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
	CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
	AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
	HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
	PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
	BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
	PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
	MODIFICATIONS.
	저작권 공지: 저작권자 © 1992-1996 The Regents of the University of California.
	모든 권리 보유.

	본 소프트웨어와 해당 문서는 어떠한 목적으로도 사용, 복사, 수정,
	배포할 수 있으며, 비용 없이 그리고 서면 합의 없이 가능합니다.
	다만, 본 소프트웨어의 모든 사본에는 위의 저작권 공지와 아래의 두 문단이
	포함되어야 합니다.

	어떠한 경우에도 캘리포니아 대학교는 이 소프트웨어와 문서의 사용으로 인해 발생하는
	직접, 간접, 특별, 우연한, 또는 결과적인 손해에 대해 책임을 지지 않습니다.
	심지어 캘리포니아 대학교가 그러한 손해의 가능성에 대해 사전에 알려져 있었더라도
	해당 책임을 지지 않습니다.

	캘리포니아 대학교는 명시적 또는 묵시적 보증을 포함하여
	어떠한 종류의 보증도 명시적이든 묵시적이든 부인합니다.
	이로 인해 제한되지 않으며 상품성 및 특정 목적에 대한 적합성을 포함합니다.
	여기에 제공된 소프트웨어는 "있는 그대로" 기반이며
	캘리포니아 대학교는 유지, 지원, 업데이트, 향상, 또는 수정의
	의무를 지지 않습니다.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

	/* Initializes semaphore SEMA to VALUE.  A semaphore is a
	nonnegative integer along with two atomic operators for
	manipulating it:

	- down or "P": wait for the value to become positive, then
	decrement it.

	- up or "V": increment the value (and wake up one waiting
	thread, if any). 

	세마포어 SEMA를 VALUE로 초기화합니다. 세마포어는
	양수의 값을 가지며, 이를 조작하기 위한 두 가지 원자적 연산이 있습니다:

	다운 또는 "P": 값을 양수로 만들 때까지 기다리고, 값을 감소시킵니다.

	업 또는 "V": 값을 증가시키고 (필요한 경우) 대기 중인 스레드 중 하나를 깨웁니다.*/
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

	/* Down or "P" operation on a semaphore.  Waits for SEMA's value
	to become positive and then atomically decrements it.

	This function may sleep, so it must not be called within an
	interrupt handler.  This function may be called with
	interrupts disabled, but if it sleeps then the next scheduled
	thread will probably turn interrupts back on. This is
	sema_down function. 
	세마포어의 Down 또는 "P" 작업. SEMA의 값을 양수로 만들 때까지 기다린 후 원자적으로 감소시킵니다.

	이 함수는 잠을 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만,
	잠을 자면 다음 예약된 스레드가 인터럽트를 다시 활성화할 수 있습니다. 이것은 sema_down 함수입니다.*/
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		//! 1.2
		// list_push_back (&sema->waiters, &thread_current ()->elem); //FIFO 방식 변경을 위해 주석처리
		list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

	/* Down or "P" operation on a semaphore, but only if the
	semaphore is not already 0.  Returns true if the semaphore is
	decremented, false otherwise.

	This function may be called from an interrupt handler.
	세마포어의 Down 또는 "P" 작업을 수행하지만 세마포어가 이미 0인 경우에만 수행합니다.
	세마포어가 감소되면 true를 반환하고, 그렇지 않으면 false를 반환합니다.

	이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

	/* Up or "V" operation on a semaphore.  Increments SEMA's value
	and wakes up one thread of those waiting for SEMA, if any.

	This function may be called from an interrupt handler. 
	세마포어의 Up 또는 "V" 작업. SEMA의 값을 증가시키고, SEMA를 기다리는 스레드 중 하나를 깨웁니다.

	이 함수는 인터럽트 핸들러에서 호출될 수 있습니다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)){
		//! 프로젝트 1.2
		list_sort(&sema->waiters, &cmp_priority, NULL);					//!waiterlist의 스레드 우선순위 변경을 고려해 우선순위 기반으로 정령

		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
	}
	sema->value++;
	//! 프로젝트 1.2
	test_max_priority();												//!선점 기능 사용

	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

	/* Self-test for semaphores that makes control "ping-pong"
	between a pair of threads.  Insert calls to printf() to see
	what's going on. 
	세마포어의 자체 테스트로, 제어를 테스트하는 스레드 쌍 사이에서 "ping-pong"을 수행합니다.
	동작을 확인하기 위해 printf() 호출을 삽입할 수 있습니다.*/
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

	/* Thread function used by sema_self_test().
	sema_test_helper 함수는 sema_self_test()에서 사용되는 스레드 함수입니다. */	
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

	/* Initializes LOCK.  A lock can be held by at most a single
	thread at any given time.  Our locks are not "recursive", that
	is, it is an error for the thread currently holding a lock to
	try to acquire that lock.

	A lock is a specialization of a semaphore with an initial
	value of 1.  The difference between a lock and such a
	semaphore is twofold.  First, a semaphore can have a value
	greater than 1, but a lock can only be owned by a single
	thread at a time.  Second, a semaphore does not have an owner,
	meaning that one thread can "down" the semaphore and then
	another one "up" it, but with a lock the same thread must both
	acquire and release it.  When these restrictions prove
	onerous, it's a good sign that a semaphore should be used,
	instead of a lock.
	
		LOCK을 초기화합니다. LOCK은 한 번에 최대 하나의 스레드에 의해 소유될 수 있습니다.
	우리의 LOCK은 "재귀적(recursive)"이 아니므로 현재 LOCK을 소유하고 있는 스레드가
	LOCK을 획득하려고 시도하는 것은 오류입니다.

	LOCK은 초기 값이 1인 세마포어의 특수화입니다. LOCK과 이러한 세마포어의 차이점은 두 가지입니다.
	첫째, 세마포어는 1보다 큰 값을 가질 수 있지만 LOCK은 한 번에 하나의 스레드만 소유할 수 있습니다.
	둘째, 세마포어는 소유자(owner)가 없습니다. 즉, 한 스레드가 세마포어를 "down"하고 다른 스레드가
	"up"할 수 있지만 LOCK의 경우 동일한 스레드가 LOCK을 획득하고 해제해야 합니다.
	이러한 제한 사항이 불편할 때는 LOCK 대신 세마포어를 사용해야 합니다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

	/* Acquires LOCK, sleeping until it becomes available if
	necessary.  The lock must not already be held by the current
	thread.

	This function may sleep, so it must not be called within an
	interrupt handler.  This function may be called with
	interrupts disabled, but interrupts will be turned back on if
	we need to sleep. 
	LOCK을 획득하며, 필요한 경우 사용 가능할 때까지 대기 상태로 들어갑니다.
	LOCK은 현재 스레드에 의해 이미 소유되어 있으면 안 됩니다.

	이 함수는 sleep할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다.
	인터럽트가 비활성화된 상태에서 호출될 수 있지만, sleep해야 할 경우 인터럽트는 다시 활성화됩니다.
	
	! 1.2 donate 추가
	lock를 다른 스레드가 점유하고 있는 경우 자신의 우선순위를 양도해 lock를 점유하는
	스레드가 우선적으로 lock를 반환하도록 한다.
	*/
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	//! 1.2 donation
	struct thread *curr =thread_current();
	if (lock->holder){					//!lock_acquire를 실행하는 스레드가 현재 실행중인 스레드이면
		curr->wait_on_lock = lock;		//!현재 스레드가 기다리고 있는 lock 입력
		list_insert_ordered(&lock->holder->donations, &curr->donation_elem,thread_compare_donate_priority, 0);
		donate_priority();
	}
	sema_down (&lock->semaphore);

	//! 1.2 donation
	curr->wait_on_lock = NULL;

	lock->holder = thread_current ();
}


	/* Tries to acquires LOCK and returns true if successful or false
	on failure.  The lock must not already be held by the current
	thread.

	This function will not sleep, so it may be called within an
	interrupt handler. 
	LOCK을 시도하고 성공하면 true를 반환하고 실패하면 false를 반환합니다.
	LOCK은 현재 스레드에 의해 이미 소유되어 있으면 안 됩니다.

	이 함수는 sleep하지 않으므로 인터럽트 핸들러 내에서 호출할 수 있습니다.*/
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

	/* Releases LOCK, which must be owned by the current thread.
	This is lock_release function.

	An interrupt handler cannot acquire a lock, so it does not
	make sense to try to release a lock within an interrupt
	handler. 
	현재 스레드에 의해 소유되고 있는 LOCK을 해제합니다.

	인터럽트 핸들러는 LOCK을 획득할 수 없으므로 인터럽트 핸들러 내에서 LOCK을 해제하려는 것은 의미가 없습니다.*/
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	//! 1.2 donation
	remove_with_lock(lock);		//!빌려준 스레드를 donations 리스트에서 제거
	refresh_priority();			//!우선순위 재설정

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

	/* Returns true if the current thread holds LOCK, false
	otherwise.  (Note that testing whether some other thread holds
	a lock would be racy.)
	현재 스레드가 LOCK을 소유하고 있는지 여부를 반환합니다.
	(다른 스레드가 LOCK을 소유하고 있는지 테스트하는 것은 경쟁 상태에 놓일 수 있음에 주의)  */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list.  리스트 내의 세마포어 하나입니다.*/
struct semaphore_elem {
	struct list_elem elem;              /* List element. 리스트 요소. */
	struct semaphore semaphore;         /* This semaphore. 이 세마포어.*/
};

	/* Initializes condition variable COND.  A condition variable
	allows one piece of code to signal a condition and cooperating
	code to receive the signal and act upon it.
	조건 변수 COND를 초기화합니다.
	조건 변수는 한 조각의 코드가 조건을 신호화하고, 협력하는 코드가 그 신호를 받아들이고
	처리할 수 있도록 해줍니다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

	/* Atomically releases LOCK and waits for COND to be signaled by
	some other piece of code.  After COND is signaled, LOCK is
	reacquired before returning.  LOCK must be held before calling
	this function.

	The monitor implemented by this function is "Mesa" style, not
	"Hoare" style, that is, sending and receiving a signal are not
	an atomic operation.  Thus, typically the caller must recheck
	the condition after the wait completes and, if necessary, wait
	again.

	A given condition variable is associated with only a single
	lock, but one lock may be associated with any number of
	condition variables.  That is, there is a one-to-many mapping
	from locks to condition variables.

	This function may sleep, so it must not be called within an
	interrupt handler.  This function may be called with
	interrupts disabled, but interrupts will be turned back on if
	we need to sleep. 
	LOCK을 원자적으로 해제하고, 다른 코드에 의해 COND가 신호화될 때까지 대기합니다.
	COND가 신호화되면 LOCK을 다시 획득한 후 반환합니다.
	이 함수를 호출하기 전에 LOCK을 보유하고 있어야 합니다.

	이 함수로 구현된 모니터는 "Mesa" 스타일이며 "Hoare" 스타일이 아닙니다.
	즉, 신호를 보내고 받는 것이 원자적인 작업이 아닙니다.
	따라서 일반적으로 대기가 완료된 후에 조건을 다시 확인하고 필요한 경우 다시 대기해야 합니다.

	특정 조건 변수는 하나의 LOCK과 연관되어 있지만, 하나의 LOCK은 여러 조건 변수와 연관될 수 있습니다.
	즉, LOCK에서 조건 변수로의 일대다 매핑이 있습니다.

	이 함수는 sleep할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다.
	이 함수는 인터럽트가 비활성화된 상태로 호출될 수 있지만, 필요한 경우에는 인터럽트가 다시 활성화됩니다.*/
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	//! 프로젝트 1.2
	// list_push_back (&cond->waiters, &waiter.elem);
	list_insert_ordered(&cond->waiters,&waiter.elem,&cmp_sem_priority,NULL);	//!우선순위 기반 줄세우기

	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

	/* If any threads are waiting on COND (protected by LOCK), then
	this function signals one of them to wake up from its wait.
	LOCK must be held before calling this function.

	An interrupt handler cannot acquire a lock, so it does not
	make sense to try to signal a condition variable within an
	interrupt handler. 
		만약 COND에 대기 중인 스레드가 있다면(LOCK으로 보호됨),
	그 중 하나에게 신호를 보내 깨우도록 합니다.
	이 함수를 호출하기 전에 LOCK을 보유하고 있어야 합니다.

	인터럽트 핸들러는 LOCK을 획득할 수 없으므로 인터럽트 핸들러 내에서 조건 변수를 신호화하려는 것은 의미가 없습니다. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
	{	
		//! 프로젝트 1.3
		list_sort(&cond->waiters, &cmp_sem_priority,NULL);	//!우선순위가 도중에 바뀌는 경우 참고하기 위해 정렬
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

	/* Wakes up all threads, if any, waiting on COND (protected by
	LOCK).  LOCK must be held before calling this function.

	An interrupt handler cannot acquire a lock, so it does not
	make sense to try to signal a condition variable within an
	interrupt handler. 
	COND에 대기 중인(LOCK으로 보호되는) 모든 스레드를 깨웁니다.
	이 함수를 호출하기 전에 LOCK을 보유하고 있어야 합니다.

	인터럽트 핸들러는 LOCK을 획득할 수 없으므로 인터럽트 핸들러 내에서 조건 변수를 신호화하려는 것은 의미가 없습니다.*/
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

//! 프로젝트 1.2에서 생성된 함수

/**
 * semaphore_elem을 받아와 해당 세마포어를 가진 스레드의 우선순위를 비교하는 함수,
*/
bool cmp_sem_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
	struct semaphore_elem *sa = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *sb = list_entry(b, struct semaphore_elem, elem);
	
	struct list_elem *sa_e = list_begin(&(sa->semaphore.waiters));
	struct list_elem *sb_e = list_begin(&(sb->semaphore.waiters));
 
	struct thread *sa_t = list_entry(sa_e, struct thread, elem);
	struct thread *sb_t = list_entry(sb_e, struct thread, elem);
 
	return (sa_t->priority) > (sb_t->priority);
	}
