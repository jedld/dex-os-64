#pragma once
#include <stddef.h>
#include <stdint.h>

// Virtual memory manager (x86_64) interface skeleton

#define PAGE_SIZE 4096ULL

typedef uint64_t pml4_t;

void vmm_init_identity(void);
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(uint64_t virt);
