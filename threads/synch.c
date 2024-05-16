/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

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
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void preemption();
bool cmp_priority_use_sema(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool priority_less_d_elem (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
void donate(struct lock *lock);
void update_priority();
// void donate(struct thread *t);
int find_max_priority_from_lock( struct lock *wlock, int priority_do );
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
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
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_insert_ordered( &sema->waiters, &thread_current ()->elem, cmp_priority, NULL );
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}
	sema->value--;

	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
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

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	// struct thread *max;
	old_level = intr_disable ();
	if (!list_empty (&sema->waiters)){
		list_sort(&sema->waiters, cmp_priority, NULL);
		// 정렬이 내림차순이니 최소값을 가져가는게 즉 최대값이 되어버림
		// struct list_elem* li_elem = list_min (&sema->waiters, cmp_priority, NULL);
		struct list_elem* li_elem = list_pop_front (&sema->waiters);
		// list_remove(li_elem);
		// printf("여기입니다. = %d ] ", list_entry(li_elem, struct thread, elem)->priority);
		thread_unblock (list_entry (li_elem, struct thread, elem));
	}
	sema->value++;

	// if(!list_empty (&sema->waiters)){
	// 	list_sort(&sema->waiters, cmp_priority, NULL);
	// 	if( max->priority > thread_current()->priority ){
	// 		thread_yield();
	// 	}
	// }
	preemption();

	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
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

/* Thread function used by sema_self_test(). */
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
   instead of a lock. */
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
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
    ASSERT (lock != NULL);
    ASSERT (!intr_context ());
    ASSERT (!lock_held_by_current_thread (lock));

	struct thread * curr = thread_current();

	// enum intr_level old_level = intr_disable();
    if (lock -> holder != NULL){
        // lock을 현재 스레드에게 저장한다.(대기중)
		curr->wait_on_lock = lock;
        list_insert_ordered(&lock->holder->donations, &curr->d_elem, cmp_priority, NULL);

        donate2(lock);
    }
	// intr_set_level(old_level);



    sema_down (&lock->semaphore);
	curr->wait_on_lock = NULL;
    lock->holder = curr;
    // lock 요청 후 실패시 도네이션
	
}
/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
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
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	enum intr_level old_level = intr_disable();
	struct thread * curr = thread_current();

	if(!list_empty( &curr->donations ) ){
		struct list_elem* temp = list_begin(&curr->donations);
		while( list_next(temp) != NULL ){
			struct thread *temp_th = list_entry( temp, struct thread, d_elem );
			if( lock == temp_th->wait_on_lock ) list_remove( &temp_th->d_elem );

			temp = list_next(temp);	
		}
		// list_sort(&thread_current()->donations, priority_less_d_elem, NULL);
	}
	
	update_priority();
	intr_set_level(old_level);

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
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
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	// 우선순위 순으로 넣도록
	
	// list_insert_ordered (&cond->waiters, &waiter.elem, cmp_priority, NULL);
	list_sort(&cond->waiters, cmp_priority_use_sema, NULL);
	list_insert_ordered (&cond->waiters, &waiter.elem, cmp_priority_use_sema, NULL);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);

}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)){
		list_sort(&cond->waiters, cmp_priority_use_sema, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}


/* 우선순위에 따라 리스트 요소 A와 B를 비교합니다. 
   A의 우선순위가 B의 우선순위보다 높으면 true를 반환합니다.
   이 함수는 list_insert_ordered의 인자로 사용됩니다. */
// bool cmp_sema_value(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
//     struct semaphore *semaphore_a = list_entry(a, struct semaphore, waiters);
//     struct semaphore *semaphore_b = list_entry(b, struct semaphore, waiters);
//     return semaphore_a->value > semaphore_b->value;
// }
    // struct thread *thread_a = list_entry(a, struct thread, elem);
    // struct thread *thread_b = list_entry(b, struct thread, elem);
    // return thread_a->priority > thread_b->priority;
// 나는 감자다.
bool cmp_priority_use_sema(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
	struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);

	struct thread *th_a = list_entry( list_max(&sema_a->semaphore.waiters, cmp_priority, NULL), struct thread, elem);
	
	struct thread *th_b = list_entry( list_max(&sema_b->semaphore.waiters, cmp_priority, NULL), struct thread, elem);

	return th_a->priority > th_b->priority;
}

/* Returns true if value A is less than value B, false
   otherwise. */
bool
priority_less_d_elem (const struct list_elem *a_, const struct list_elem *b_,
            void *aux UNUSED) 
{
  int a = list_entry (a_, struct thread, d_elem)->priority;
  int b = list_entry (b_, struct thread, d_elem)->priority;
  
  return a > b;
}

// void donate(struct thread *t)
// {
//     while (t -> wait_on_lock != NULL && t -> wait_on_lock -> holder != NULL && t->priority > t->wait_on_lock->holder->priority)
//     {
//         t->wait_on_lock->holder->priority = t->priority;
//         t = t->wait_on_lock->holder;
//     }
// }

void donate2( struct lock * lock ){

	int max_priority = lock->holder->priority;
	
	for (struct list_elem* temp = list_begin (&lock->holder->donations); temp!=NULL&&temp != list_end(&lock->holder->donations); temp = list_next(temp))
	{
		struct thread * temp_th = list_entry(temp, struct thread, d_elem);
		max_priority = temp_th->priority > max_priority?temp_th->priority:max_priority;
	}
	lock->holder->priority = max_priority;
}

void redonate2( struct lock* lock ){



}

void update_priority()
{
	struct thread *curr_thread = thread_current();

	curr_thread->priority = curr_thread->origin_priority;

	if (!list_empty(&curr_thread->donations))		// Consider Multiple Donation
	{
		struct thread *highest_donations = list_entry(list_front(&curr_thread->donations), struct thread, d_elem);

		if (highest_donations->priority > curr_thread->priority)
			curr_thread->priority = highest_donations->priority;
	}	
}

void donate(struct lock *lock){
	int max_priority = list_entry(list_max(&lock->holder->donations, priority_less_d_elem, NULL), struct thread, d_elem)->priority;
	
	
	// 도네이션 리스트가 있다면
	if( lock->holder->wait_on_lock!= NULL ){
		max_priority = find_max_priority_from_lock(&lock->holder->wait_on_lock, max_priority);
	}
	// while 
	
	if ( max_priority > lock->holder->priority ){
		lock->holder->origin_priority = lock->holder->priority;
		lock->holder->priority = max_priority;
	}
	// lock->semaphore.waiters

}

int find_max_priority_from_lock( struct lock *wlock, int priority_do ){
	int max_priority = priority_do;
	// printf("function in %d", max_priority);
	if ( wlock->holder->wait_on_lock != NULL ){
		int temp_priority = find_max_priority_from_lock( wlock->holder->wait_on_lock, wlock->holder->priority );

		max_priority = ( temp_priority > max_priority ) ? temp_priority : max_priority;

		wlock->holder->priority = max_priority;
	}else{
		if (wlock->holder->priority > max_priority ){
			max_priority = wlock->holder->priority;
		}
	}
	// printf(" : function out %d\n", max_priority);
	return max_priority;
}
