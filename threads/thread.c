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

static struct list block_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

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

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* CPU의 스택 포인터 `rsp'를 읽은 다음,
이를 페이지의 시작 부분으로 내립니다. `struct thread'는
항상 페이지의 시작 부분에 있고 스택 포인터는 중간 어딘가에 있기 때문에,
이는 현재 스레드를 찾아냅니다. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* 현재 실행 중인 코드를 스레드로 변환함으로써 스레딩 시스템을 초기화합니다. 
일반적으로 이것은 작동할 수 없으며, 
이 경우에만 가능한 이유는 loader.S가 스택의 하단을 페이지 경계에 주의 깊게 놓았기 때문입니다.

또한 실행 큐와 tid 잠금도 초기화합니다.

이 함수를 호출한 후, thread_create()로 어떤 스레드도 생성하기 전에 반드시 페이지 할당자를 초기화해야 합니다.

이 함수가 완료될 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&block_list);
	list_init (&destruction_req);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

void print_ready_list() {
  if (list_empty(&ready_list)) {
    // printf("empty list\n");
    return;
  }

  struct list_elem *tail = list_tail(&ready_list);
  struct list_elem *curr_list = list_front(&ready_list);
  struct thread *curr;

  printf("(ready-list) ");
  while (curr_list != tail) {
    curr = list_entry(curr_list, struct thread, elem);
    printf("%s ", curr->name);
    curr_list = list_next(curr_list);
  }
  printf("\n");
}

/* 타이머 인터럽트 핸들러에 의해 각 타이머 틱마다 호출됩니다.
따라서, 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();
	print_ready_list();
	// printf("idle_thread : %s, thread_current : %s\n", idle_thread->name, t->name);
	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;
	

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* 주어진 초기 PRIORITY로 NAME이라는 이름의 새로운 커널 스레드를 생성하며, 
AUX를 인자로하여 FUNCTION을 실행하고, 그것을 준비 큐에 추가합니다. 
생성된 새 스레드의 스레드 식별자를 반환하거나, 생성이 실패하면 TID_ERROR를 반환합니다.

thread_start()가 호출되었다면, 새로운 스레드는 thread_create()가 반환하기 전에 
스케줄링될 수 있습니다. 심지어 thread_create()가 반환하기 전에 종료될 수도 있습니다. 
반대로, 원래 스레드는 새 스레드가 스케줄링되기 전에 얼마든지 실행될 수 있습니다. 
순서를 보장하기 위해 세마포어나 다른 형태의 동기화를 사용해야 합니다.

제공된 코드는 새 스레드의 `priority' 멤버를 PRIORITY로 설정하지만, 
실제 우선 순위 스케줄링은 구현되지 않았습니다. 우선 순위 스케줄링은 문제 1-3의 목표입니다. */
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

	/* Add to run queue. */
	thread_unblock (t);

	return tid;
}

/* 현재 스레드가 잠듭니다. 
thread_unblock()에 의해 깨어날 때까지 다시 스케줄링되지 않습니다.
이 함수는 인터럽트가 꺼진 상태에서 호출되어야 합니다. 
대체로 synch.h 내의 동기화 프리미티브 중 하나를 사용하는 것이 더 좋은 방법입니다. */
void
thread_block (void) { 
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* 차단된 스레드 T를 실행 준비 상태로 전환합니다.
T가 차단되지 않은 상태에서 이 함수를 호출하는 것은 오류입니다. 
(실행 중인 스레드를 실행 준비 상태로 만들기 위해서는 thread_yield()를 사용하세요.)

이 함수는 실행 중인 스레드의 실행을 선점하지 않습니다. 
이것은 중요할 수 있습니다: 호출자가 직접 인터럽트를 비활성화했었다면, 
스레드를 원자적으로 차단 해제하고 다른 데이터를 업데이트 할 수 있을 것으로 예상할 수 있습니다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	list_push_back (&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* 실행 중인 스레드를 반환합니다.
이것은 running_thread()에 몇 가지 정상성 검사가 추가된 것입니다.
자세한 내용은 thread.h 상단의 큰 주석을 참조하세요. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* T가 정말로 스레드인지 확인하세요.
이러한 주장 중 하나라도 발생하면, 스레드가 스택을 넘친 것일 수 있습니다.
각 스레드는 4kB 미만의 스택을 가지고 있으므로, 
몇 개의 큰 자동 배열이나 적당한 재귀는 스택 오버플로를 일으킬 수 있습니다. */
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

	/* 우리의 상태를 'THREAD_DYING'으로 설정하고 다른 프로세스를 스케줄링하십시오.
우리는 schedule_tail() 호출 중에 파괴될 것입니다. */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* CPU를 양보합니다. 현재 스레드는 잠들지 않고 스케줄러의 판단에 따라 즉시 다시 스케줄될 수 있습니다 */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level; // 현재 인터럽트 레벨 저장

	ASSERT (!intr_context ()); // 인터럽트 컨텍스트에서 호출되지않았는지 확인 (호출되지않았으면 함수실행)

	old_level = intr_disable (); // 현재 인터럽트를 비활성화
	if (curr != idle_thread) // 현재 스레드가 비활성상태가 아니라면 (공회전)
		list_push_back (&ready_list, &curr->elem); // 현재 스레드를 리스트의 back으로 보냄.

	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
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
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* 유휴 스레드입니다. 다른 스레드가 실행 준비가 되어 있지 않을 때 실행됩니다.

유휴 스레드는 처음에 thread_start()에 의해 준비 리스트에 올려집니다. 
초기에 한 번 스케줄될 것이며, 이 시점에서 idle_thread를 초기화하고, 
스레드 시작이 계속될 수 있도록 전달된 세마포어에 "up" 작업을 수행한 후 즉시 차단됩니다. 
그 후에 유휴 스레드는 준비 리스트에 나타나지 않습니다. 
준비 리스트가 비어 있을 때 next_thread_to_run()에 의해 특별한 경우로 반환됩니다. */

static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* 다음 인터럽트를 기다리며 인터럽트를 다시 활성화합니다.

		sti 명령어는 다음 명령어가 완료될 때까지 인터럽트를 비활성화하므로, 
		이 두 명령어는 원자적으로 실행됩니다. 
		이 원자성은 중요합니다. 그렇지 않으면, 인터럽트를 다시 활성화하는 사이에 인터럽트가 처리될 수 있으며, 
		다음 인터럽트가 발생하기를 기다리는 동안 최대 한 클럭 틱의 시간을 낭비할 수 있습니다.

		[IA32-v2a]의 "HLT", [IA32-v2b]의 "STI" 및 
		[IA32-v3a] 7.11.1 "HLT 명령어"를 참조하세요.. */
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


/* T를 NAME이라는 이름의 차단된 스레드로 기본 초기화합니다. */
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
}

/* 
다음에 스케줄링될 스레드를 선택하고 반환합니다. 
실행 큐가 비어 있지 않다면 실행 큐에서 스레드를 반환해야 합니다. 
(현재 실행 중인 스레드가 계속 실행될 수 있다면, 그 스레드는 실행 큐에 있을 것입니다.) 
만약 실행 큐가 비어 있다면, idle_thread를 반환합니다.
 */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
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

	// if(curr != idle_thread){
	// list_push_back(&block_list, curr);
	// }

static void
thread_launch (struct thread *th) { // 컨텍스트 스위칭
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

// void insert_blockList(void){
// 	ASSERT (!intr_context ());
// 	ASSERT (intr_get_level () == INTR_OFF);
// 	thread_current ()->status = THREAD_BLOCKED;
// 	list_push_back(&block_list, )
// }