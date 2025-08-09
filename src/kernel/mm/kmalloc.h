#pragma once
#include <stddef.h>
#include <stdint.h>

void kmalloc_init(void* heap_start, size_t heap_size);
void* kmalloc(size_t size);
void kfree(void* ptr);
size_t kmalloc_usable_size(void* ptr);
