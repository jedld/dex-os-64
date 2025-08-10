#pragma once
#include <stddef.h>
#include <stdint.h>

// Physical memory manager (PMM) interface
// Frame size: 4096 bytes

#define PMM_FRAME_SIZE 4096ULL

void pmm_init(void* mb2_info_or_uefi_map, int from_uefi);
uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);
uint64_t pmm_total_physical_bytes(void);

// Allocate 'count' contiguous frames; returns physical address or 0 on failure
uint64_t pmm_alloc_frames(size_t count);
void pmm_free_frames(uint64_t paddr, size_t count);
// Allocate contiguous frames such that the returned base + size <= max_phys (exclusive). Returns 0 on failure.
uint64_t pmm_alloc_frames_below(size_t count, uint64_t max_phys_exclusive);

// Reserve a physical range (rounded to frames) so allocator won't hand it out
void pmm_reserve(uint64_t paddr, uint64_t size);
