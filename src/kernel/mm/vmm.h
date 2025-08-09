#pragma once
#include <stddef.h>
#include <stdint.h>

// Virtual memory manager (x86_64) interface

#define PAGE_SIZE 4096ULL

typedef uint64_t pml4_t;

// Page table flags
#define VMM_PRESENT   (1ULL<<0)
#define VMM_RW        (1ULL<<1)
#define VMM_US        (1ULL<<2)
#define VMM_PWT       (1ULL<<3)
#define VMM_PCD       (1ULL<<4)
#define VMM_ACCESSED  (1ULL<<5)
#define VMM_DIRTY     (1ULL<<6)
#define VMM_PS        (1ULL<<7)  // for PD/PT entries (2MiB/1GiB)
#define VMM_GLOBAL    (1ULL<<8)
#define VMM_NX        (1ULL<<63)

// Kernel higher-half base (choose 0xFFFF800000000000 typical for 48-bit)
#ifndef KERNEL_BASE
#define KERNEL_BASE 0xFFFFFFFF80000000ULL
#endif

// Initialize paging structures with identity map for low memory used by boot
void vmm_init_identity(void);

// Switch to a new PML4
void vmm_load_cr3(uint64_t pml4_phys);
int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(uint64_t virt);

// Translate a virtual address to phys if mapped; returns 1 on success
int vmm_virt_to_phys(uint64_t virt, uint64_t* out_phys);
