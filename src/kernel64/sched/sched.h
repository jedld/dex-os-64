#pragma once
#include <stdint.h>

int sched_create(void (*entry)(void*), void* arg);
void sched_yield(void);
void sched_start(void);

// Lightweight thread info for diagnostics (no internal pointers exposed)
typedef struct {
	int id;         // thread id (0..)
	int state;      // 0=ready,1=running,2=done
	uint64_t rsp;   // saved stack pointer
} sched_thread_info_t;

// Enumerate up to 'max' threads into 'out'. Returns the number written.
int sched_enumerate(sched_thread_info_t* out, int max);

// Get currently running thread id, or -1 if scheduler not started.
int sched_current_id(void);
