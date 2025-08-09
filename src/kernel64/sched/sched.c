// Minimal cooperative scheduler for x86_64 kernel threads (single core for now)
#include <stdint.h>
#include <stddef.h>
#include "../console.h"

typedef struct thread {
	struct thread* next;
	uint64_t rsp;      // saved stack pointer
	void (*entry)(void*);
	void* arg;
	int state;         // 0=ready,1=running,2=done
} thread_t;

static thread_t* g_runq = NULL;
static thread_t* g_current = NULL;

// Simple static storage for threads and stacks
#define MAX_THREADS 8
#define STACK_SIZE  (16*1024)
static thread_t g_threads[MAX_THREADS];
static uint8_t g_stacks[MAX_THREADS][STACK_SIZE] __attribute__((aligned(16)));
static int g_thread_count = 0;

extern void sched_context_switch(uint64_t* old_rsp, uint64_t new_rsp);

static void enqueue(thread_t* t) {
	t->next = NULL;
	if (!g_runq) { g_runq = t; return; }
	thread_t* p = g_runq; while (p->next) p = p->next; p->next = t;
}

static thread_t* dequeue(void) {
	thread_t* t = g_runq; if (t) g_runq = t->next; return t;
}

static void thread_trampoline(void) {
	// On entry, g_current is this thread; its entry and arg are set
	void (*fn)(void*) = g_current->entry;
	void* arg = g_current->arg;
	fn(arg);
	g_current->state = 2;
	// Pick next and switch
	thread_t* next = dequeue();
	if (!next) {
		console_write("[sched] no next thread, halting\n");
		for(;;) { __asm__ volatile ("hlt"); }
	}
	thread_t* prev = g_current;
	g_current = next; next->state = 1;
	sched_context_switch(&prev->rsp, next->rsp);
}

int sched_create(void (*entry)(void*), void* arg) {
	if (g_thread_count >= MAX_THREADS) return -1;
	thread_t* t = &g_threads[g_thread_count];
	t->entry = entry; t->arg = arg; t->state = 0; t->next = NULL;
	// Set up stack: push return RIP = thread_trampoline end (never returns)
	uint8_t* stack_top = g_stacks[g_thread_count] + STACK_SIZE;
	stack_top = (uint8_t*)((uintptr_t)stack_top & ~0xFULL);
	// Build initial stack frame: return address = thread_trampoline
	uint64_t* sp = (uint64_t*)stack_top;
	*(--sp) = (uint64_t)thread_trampoline; // RIP for iret-like simulation
	// Our context switch will 'ret' into this address
	t->rsp = (uint64_t)sp;
	enqueue(t);
	++g_thread_count;
	return 0;
}

void sched_yield(void) {
	if (!g_current) return;
	thread_t* next = dequeue();
	if (!next) return; // nothing else ready
	if (next == g_current) { enqueue(next); return; }
	// round-robin: enqueue current if still runnable
	if (g_current->state == 1) { g_current->state = 0; enqueue(g_current); }
	thread_t* prev = g_current;
	g_current = next; next->state = 1;
	sched_context_switch(&prev->rsp, next->rsp);
}

void sched_start(void) {
	if (g_current) return;
	thread_t* next = dequeue();
	if (!next) { console_write("[sched] empty runq\n"); return; }
	g_current = next; next->state = 1;
	uint64_t dummy = 0; // not used after switch
	sched_context_switch(&dummy, next->rsp);
}

// Assembly context switch: save current RSP into *old, load new_rsp, then 'ret'
__attribute__((naked)) void sched_context_switch(uint64_t* old_rsp, uint64_t new_rsp) {
	__asm__ volatile (
		"mov %rsp, (%rdi)\n\t"  // *old_rsp = rsp
		"mov %rsi, %rsp\n\t"    // rsp = new_rsp
		"ret\n\t"
	);
}
