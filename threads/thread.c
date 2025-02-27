#include "threads/thread.h"
#include "threads/fixed_point_arithmetic.h"
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

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* thread recarculate target */
static struct list useable_all_threads;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

static int load_avg; // 부하 평균

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Custom Function */
void thread_sleep(int64_t ticks);
void thread_wakeup(int64_t ticks);
int64_t get_global_ticks(void);
void update_global_ticks(int64_t ticks);
int64_t find_min_less (struct list_elem *e,struct list_elem *min, int64_t global_tick );

static struct list sleep_list;
int64_t global_ticks;
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool cmp_priority_delem(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool cmp_priority_all_th_elem(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void update();
void recarculate_priority();
int carculate_priority( int recent_cpu, int nice );
void recarculate_recent_cpu();
int carcluate_recent_cpu( int recent_cpu, int nice );
void increase_recent_cpu();
void carculate_load_avg();


/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
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
   finishes. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). 
	 * 임시 GDT를 커널용으로 다시 로드, 이 GDT에는 사용자 컨텍스트가 포함되지 않음. 
	 * 즉, 커널이 사용자 컨텍스트와 함께 GDT를 다시 만들 것 */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&sleep_list);
	list_init (&destruction_req);
	list_init (&useable_all_threads);

	load_avg = 0; // 부하 평균 초기화
	global_ticks = INT64_MAX;
	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	list_push_back(&useable_all_threads,  &initial_thread->useable_th_elem);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
	// initial_thread->local_ticks = 0;
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);
	// thread_create ("idle", PRI_DEFAULT, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
// 각 타이머 틱마다 호출되는 함수
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)					// idle 스레드인 경우
		idle_ticks++;			
#ifdef USERPROG								// user program 스레드인 경우
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;			     		// 그 외

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)		// 스레드가 time slice를 초과하면
		intr_yield_on_return ();			// CPU 양보 요청
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
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();
	// printf("티아이디 %d\n",tid);
	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;
	list_push_back(&useable_all_threads,  &t->useable_th_elem);
	// printf("Thread Name: %s/n", list_entry(list_begin(&useable_all_threads),struct thread, useable_th_elem)->name);
	
	// list_init(&t->d_elem);
	/* Add to run queue. */
	thread_unblock (t);
	// list_push_back(&useable_all_threads,  &t->useable_th_elem);
    /* 
     * compare the priorities of the currently running thread and the newly inserted one.
     * Yield the CPU if the newly arriving thread has higher priority    
     */
	if (thread_current()->priority < t->priority) {
		thread_yield();
	}
	// printf("useable_all_threads %d in \n", list_entry(list_begin(&useable_all_threads),struct thread, useable_th_elem)->priority);
	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
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
   update other data. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	// list_push_back (&ready_list, &t->elem); // 
	list_insert_ordered(&ready_list, &t->elem, cmp_priority, NULL);
	// list_push_back() 하고 list_sort() 하는 게 아니라, 처음부터 정렬해서 삽입
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	list_remove(&thread_current()->useable_th_elem);
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();		// 현재 스레드 정보 저장
	enum intr_level old_level;						// 현재 인터럽트 활성화 상태 체크

	ASSERT (!intr_context ());						
	old_level = intr_disable ();					// 인터럽트 비활성화
	if (curr != idle_thread){
		// idle thread가 아니라면
		list_insert_ordered(&ready_list, &curr->elem, cmp_priority, NULL); // insert_ordered
	}						
	do_schedule (THREAD_READY);						// 스케쥴러 호출
	intr_set_level (old_level);						// 이전에 저장한 인터럽트 레벨 복원
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	if (thread_mlfqs) return;

	thread_current ()->priority = new_priority;	// set priority of the current thread
	thread_current ()->origin_priority = new_priority;	// set priority of the current thread

	// if (list_empty(&ready_list) ) return;
	struct thread *max = list_entry( list_max( &ready_list, cmp_priority , NULL ), struct thread, elem );

	update();
	preemption();

	// list_sort(&ready_list, cmp_priority, NULL);	// reorder the ready_list
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
	enum intr_level old_level = intr_disable ();
	thread_current()->nice = nice;
	intr_set_level(old_level);
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	enum intr_level old_level = intr_disable ();
	int nice = thread_current()->nice;
	intr_set_level(old_level);
	return nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	enum intr_level old_level = intr_disable ();
	int load_multi = convert_int_near( fixed_mul_by_int( load_avg, 100 ) );
	intr_set_level(old_level);
	return load_multi;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	enum intr_level old_level = intr_disable ();
	int recent_multi = convert_int_near( fixed_mul_by_int( thread_current()->recent_cpu, 100 ) );
	intr_set_level(old_level);
	return recent_multi;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
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
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
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
	// 중요 매우 중요 나는 감자다 2
	list_init(&t->donations);
	t->wait_on_lock = NULL;
	t->origin_priority = priority;
	// init _ mlfq variable
	t->recent_cpu = 0;
	t->nice = 0;
	
	// list_push_back(&useable_all_threads,  &t->useable_th_elem);
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else{
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
	}
}

/* Use iretq to launch the thread */
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
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
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
			"call __next\n"         // read the current rip.
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
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);											// 인터럽트가 비활성화 상태인지 체크
	ASSERT (thread_current()->status == THREAD_RUNNING);							// 실행 중인 스레드인지 체크
	while (!list_empty (&destruction_req)) {										// 종료된 스레드들 리스트 확인해서
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);	// 스레드 구조체로 변환해서
		palloc_free_page(victim);													// 삭제
	}
	thread_current ()->status = status;												// 현재 스레드 상태 변경해주고,
	schedule ();																	// 스케쥴링
}

static void
schedule (void) {
	struct thread *curr = running_thread ();							// 현재 스레드 저장
	struct thread *next = next_thread_to_run ();						// 다음 실행할 스레드 저장

	ASSERT (intr_get_level () == INTR_OFF);				
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));				
	/* Mark us as running. */
	next->status = THREAD_RUNNING;								

	/* Start new time slice. */	
	thread_ticks = 0;													// 새로운 스레드를 실행할 때 타임 슬라이스 계산을 새로 시작해야 함!!								

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
			// all_thread_list 에서 삭제
			// list_remove( &curr->useable_th_elem ); 
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/* =========== Custom Function =========== */
void thread_sleep(int64_t ticks)
{
	/* When you manipulate thread list, disable interrupt! */
	// 스레드 목록을 조작하는 동안 인터럽트가 발생하면 안됨 -> 인터럽트 비활성화
	struct thread *curr = thread_current();
	enum intr_level old_level;

	old_level = intr_disable();

	/* 1. 만약 현재 스레드가 idle thread가 아니라면 BLOCKED로 상태 변경
	   2. 깨워야 하는 시간, 즉 local ticks 값 저장 -> 최솟값 업데이트??
	   3. schedule 함수 호출 (스레드 스케쥴링) */
	if (curr != idle_thread) {
		list_push_back (&sleep_list, &curr->elem);		
		curr->status = THREAD_BLOCKED;
		curr->local_ticks = ticks;						// 재울 시간 저장
	}
	update_global_ticks(ticks);							// 최소 tick 값 갱신
	schedule();
	intr_set_level(old_level);
}

void thread_wakeup(int64_t ticks)
{
	enum intr_level old_level;

	if(!list_empty(&sleep_list))
	{
		// 리스트에서 local_tick이 최소인 스레드 가져옴
		struct thread *min_thread = list_entry(list_min(&sleep_list, find_min_less, NULL), struct thread, elem);
		
		while( min_thread->local_ticks <= ticks ){
			
			old_level = intr_disable(); // 인터럽트 비활성화
				
			min_thread->status = THREAD_READY;		
			list_remove(&min_thread->elem);
			// list_push_back(&ready_list, &min_thread->elem);
			list_insert_ordered(&ready_list, &min_thread->elem, cmp_priority, NULL);
			intr_set_level(old_level);
			min_thread = list_entry(list_min(&sleep_list, find_min_less, NULL), struct thread, elem);
		}
	}
}

int64_t get_global_ticks(void)
{
	return global_ticks;
}

void update_global_ticks(int64_t ticks)
{
	global_ticks = global_ticks > ticks ? global_ticks : ticks;
}

int64_t find_min_less (struct list_elem *e,struct list_elem *min, int64_t global_tick )
{
	int64_t a = list_entry(e, struct thread, elem)->local_ticks;
	int64_t b = list_entry(min, struct thread, elem)->local_ticks;

	return a < b;
}

/* 우선순위에 따라 리스트 요소 A와 B를 비교합니다. 
   A의 우선순위가 B의 우선순위보다 높으면 true를 반환합니다.
   이 함수는 list_insert_ordered의 인자로 사용됩니다. */
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
    struct thread *thread_a = list_entry(a, struct thread, elem);
    struct thread *thread_b = list_entry(b, struct thread, elem);
    return thread_a->priority > thread_b->priority;
}


void preemption()
{
	enum intr_level old_level = intr_disable();
	// list_sort(&ready_list, cmp_priority, NULL);

	struct thread *cur = thread_current();
	
	// if (!list_empty(&ready_list) && cur->priority < list_entry(list_max(&ready_list, cmp_priority, NULL), struct thread, elem)->priority)
	if (!list_empty(&ready_list) && cur->priority < list_entry(list_front(&ready_list), struct thread, elem)->priority)
	{
		thread_yield();
	}
	intr_set_level(old_level);
}

/* customed */
void update()
{
	thread_current()->priority = thread_current()->origin_priority;
	if (!list_empty(&thread_current()->donations)) 
	{
		list_sort(&thread_current()->donations, cmp_priority_delem, NULL);
		struct list_elem *e = list_front(&thread_current()->donations);
		struct thread *max_t = list_entry(e, struct thread, d_elem);
		// printf("\n*** cur pri: %d, max pri: %d \n", thread_current()->priority, max_t->priority);

		if (thread_current()->priority < max_t->priority) {
			thread_current()->priority = max_t->priority;
			return;
		}
	}
}


/* mlfq */
void recarculate_priority(){
	// if( thread_current() != idle_thread ){
		if( !list_empty(&useable_all_threads) ){
			
			for (struct list_elem* temp = list_begin(&useable_all_threads); temp != list_end(&useable_all_threads); temp = list_next(temp))
			{
				struct thread* temp_th = list_entry(temp, struct thread, useable_th_elem);
				if( temp_th != idle_thread ){
					temp_th->priority = carculate_priority( temp_th->recent_cpu, temp_th->nice );
				}	
			}

		}
	// }
}

int carculate_priority( int recent_cpu, int nice ){
	return PRI_MAX - convert_int_near( fixed_div_by_int(recent_cpu,4))  - (nice * 2);
}

void recarculate_recent_cpu(){
	if( !list_empty(&useable_all_threads) ){
		// (ready_list_cnt + 1) == 준비중인 스레드 수 + 현재 러닝 스레드
		
		

		for ( struct list_elem* temp = list_begin(&useable_all_threads); temp != list_end(&useable_all_threads); temp = list_next(temp) )
		{
			struct thread* temp_th = list_entry(temp, struct thread, useable_th_elem);
			if( temp_th != idle_thread ){
				temp_th->recent_cpu = carcluate_recent_cpu( temp_th->recent_cpu, temp_th->nice);
			}		
		}
		
	}
}

int carcluate_recent_cpu( int recent_cpu, int nice ){
	int decay = fixed_div(fixed_mul_by_int(load_avg,2) , fixed_add_int(fixed_mul_by_int(load_avg, 2),1) );
	return fixed_add_int( fixed_mul(decay , recent_cpu), nice );
}

void increase_recent_cpu(){
	if(thread_current() != idle_thread){
		struct thread *current = thread_current();
		current->recent_cpu = fixed_add_int(current->recent_cpu, 1);
	}
}

void carculate_load_avg(){
	int ready_list_cnt = list_size(&ready_list);
	if(thread_current() != idle_thread){
		ready_list_cnt++;		
	}
	load_avg = fixed_add( fixed_mul(fixed_div(convert_fixed(59),convert_fixed(60)),load_avg ) ,  fixed_mul_by_int( fixed_div (convert_fixed(1),convert_fixed(60)),ready_list_cnt ));
}

// bool cmp_priority_all_th_elem(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
//     struct thread *thread_a = list_entry(a, struct thread, useable_th_elem);
//     struct thread *thread_b = list_entry(b, struct thread, useable_th_elem);
//     return thread_a->priority > thread_b->priority;
// }