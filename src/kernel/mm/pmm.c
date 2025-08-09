#include "pmm.h"

// Placeholder PMM: counters only. Replace with bitmap implementation using
// the Multiboot2 memory map (or UEFI map) during early boot.

static uint64_t g_total = 0;
static uint64_t g_free = 0;

void pmm_init(void* info, int from_uefi) {
    (void)info; (void)from_uefi;
    // TODO: Parse memory map and initialize bitmap of frames
    // For now, pretend we have 128 MiB with 64 MiB free
    g_total = 128ULL * 1024 * 1024;
    g_free  =  64ULL * 1024 * 1024;
}

uint64_t pmm_total_bytes(void) { return g_total; }
uint64_t pmm_free_bytes(void) { return g_free; }

uint64_t pmm_alloc_frames(size_t count) {
    uint64_t bytes = (uint64_t)count * PMM_FRAME_SIZE;
    if (g_free < bytes) return 0;
    g_free -= bytes;
    // TODO: return actual physical frame start
    return 0x100000; // dummy address
}

void pmm_free_frames(uint64_t paddr, size_t count) {
    (void)paddr;
    uint64_t bytes = (uint64_t)count * PMM_FRAME_SIZE;
    g_free += bytes;
}
