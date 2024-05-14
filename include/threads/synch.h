#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
struct semaphore {
	unsigned value;             /* Current value. */
	struct list waiters;        /* List of waiting threads. */
};

void sema_init (struct semaphore *, unsigned value);	// 세마포어 초기화
void sema_down (struct semaphore *);	// 세마포어 요청, 세마포어를 얻으면 value를 1만큼 감소
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);		// 세마포어를 release하고 value를 1만큼 증가
void sema_self_test (void);

/* Lock. */
struct lock {
	struct thread *holder;      /* Thread holding lock (for debugging). */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition {
	struct list waiters;        /* List of waiting threads. */
};

void cond_init (struct condition *);	// CV data structure 초기화
void cond_wait (struct condition *, struct lock *);		// CV에 의한 시그널을 기다림
void cond_signal (struct condition *, struct lock *);	// 우선순위가 가장 높은 쓰레드에게 시그널을 보냄, CV에서 기다리는 쓰레드 중에
void cond_broadcast (struct condition *, struct lock *);	// 모든 쓰레드에게 시그널을 보냄, CV에서 기다리는 쓰레드 중에

/* Optimization barrier.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")



#endif /* threads/synch.h */
