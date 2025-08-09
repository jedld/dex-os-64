#pragma once
#include <stdint.h>

int sched_create(void (*entry)(void*), void* arg);
void sched_yield(void);
void sched_start(void);
