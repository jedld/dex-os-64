// pmm.c — Physical Memory Manager using a simple bitmap over available regions
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>
// Use the shared Multiboot2 tag definitions to avoid ID mismatches
#include "../../kernel64/mb2.h"

// Simple region list of usable physical memory from Multiboot2
typedef struct {
    uint64_t base;
    uint64_t len;
} region_t;

#define MAX_REGIONS 64
static region_t g_regions[MAX_REGIONS];
static uint32_t g_region_count = 0;

// One global bitmap that covers from lowest usable frame to highest usable frame.
// Bit=1 means allocated/reserved, 0 means free.
static uint64_t g_bitmap_base = 0;      // lowest usable phys addr
static uint64_t g_bitmap_limit = 0;     // one past highest usable phys addr
static uint64_t g_total_phys = 0;
static uint64_t g_total_usable = 0;
static uint64_t g_free = 0;

static uint8_t* g_bitmap = NULL;        // stored in .bss; allocated at compile time? We'll place it statically sized.

#ifndef PMM_MAX_FRAMES
#define PMM_MAX_FRAMES (1024*1024) // up to 4 GiB / 4 KiB
#endif
static uint8_t g_bitmap_storage[PMM_MAX_FRAMES/8];

// Note: mb2.h provides the correct tag IDs and structures.

static inline uint64_t align_down(uint64_t x, uint64_t a) { return x & ~(a-1); }
static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + (a-1)) & ~(a-1); }

static void add_region(uint64_t base, uint64_t len) {
    if (len == 0 || g_region_count >= MAX_REGIONS) return;
    // Clamp region to a reasonable upper bound (4GiB) to match early identity map
    const uint64_t cap = 4ULL * 1024 * 1024 * 1024; // 4GiB
    if (base >= cap) return;
    uint64_t end = base + len;
    if (end > cap) end = cap;
    if (end <= base) return;
    g_regions[g_region_count++] = (region_t){ base, end - base };
}

static void parse_mb2(void* info, int from_uefi) {
    if (!info) return;
    const uint8_t* base = (const uint8_t*)info;
    uint32_t total_size = *(const uint32_t*)base;
    const mb2_tag* tag = (const mb2_tag*)(base + 8); // skip total_size + reserved

    int saw_efi = 0, saw_mmap = 0;
    // Prefer EFI memory map only when booted via UEFI
    if (from_uefi) {
        for (const mb2_tag* t = tag; (const uint8_t*)t < base + total_size && t->type != MB2_TAG_END; t = mb2_next_tag(t)) {
            if (t->type == MB2_TAG_EFI_MMAP) {
                const mb2_tag_efi_mmap_hdr* em = (const mb2_tag_efi_mmap_hdr*)t;
                uint32_t esize = em->desc_size;
                uint32_t count = mb2_efi_desc_count(em);
                const uint8_t* ep = (const uint8_t*)mb2_efi_first_desc(em);
                for (uint32_t i = 0; i < count; ++i) {
                    const efi_mem_desc* d = (const efi_mem_desc*)(ep + i * esize);
                    uint64_t bytes = d->NumberOfPages * 4096ULL;
                    // Track totals conservatively within 4GiB cap
                    if (d->Type == 7 /*EfiConventionalMemory*/) {
                        uint64_t s = align_up(d->PhysicalStart, PMM_FRAME_SIZE);
                        uint64_t end = align_down(d->PhysicalStart + bytes, PMM_FRAME_SIZE);
                        add_region(s, end > s ? (end - s) : 0);
                    }
                }
                saw_efi = 1;
                break;
            }
        }
    }
    if (!saw_efi) {
        for (const mb2_tag* t = tag; (const uint8_t*)t < base + total_size && t->type != MB2_TAG_END; t = mb2_next_tag(t)) {
            if (t->type == MB2_TAG_MMAP) {
                const mb2_tag_mmap_hdr* mm = (const mb2_tag_mmap_hdr*)t;
                const uint8_t* p = (const uint8_t*)mm;
                uint32_t entry_size = *(const uint32_t*)(p + 8);
                uint32_t offset = 16;
                while (offset + entry_size <= mm->tag.size) {
                    const mb2_mmap_entry* e = (const mb2_mmap_entry*)(p + offset);
                    if (e->type == 1 /* available */) {
                        uint64_t s = align_up(e->base_addr, PMM_FRAME_SIZE);
                        uint64_t end = align_down(e->length + e->base_addr, PMM_FRAME_SIZE);
                        add_region(s, end > s ? (end - s) : 0);
                    }
                    offset += entry_size;
                }
                saw_mmap = 1;
            }
        }
    }
    // Fallback to basic meminfo if neither detailed map was present
    if (!saw_efi && !saw_mmap) {
        for (const mb2_tag* t = tag; (const uint8_t*)t < base + total_size && t->type != MB2_TAG_END; t = mb2_next_tag(t)) {
            if (t->type == MB2_TAG_BASIC_MEMINFO) {
                const mb2_tag_basic_meminfo* bi = (const mb2_tag_basic_meminfo*)t;
                uint64_t lower = (uint64_t)bi->mem_lower * 1024ULL;
                uint64_t upper = (uint64_t)bi->mem_upper * 1024ULL;
                // Treat only upper memory (above 1MiB) as usable; cap via add_region
                if (upper > 0) {
                    uint64_t base1 = 0x100000ULL;
                    add_region(base1, upper);
                }
                break;
            }
        }
    }
    // Recompute totals from the clamped region list
    g_total_phys = 0; g_total_usable = 0;
    for (uint32_t i = 0; i < g_region_count; ++i) {
        g_total_phys += g_regions[i].len;
        g_total_usable += g_regions[i].len;
    }
}

static void build_bitmap_bounds(void) {
    if (g_region_count == 0) { g_bitmap_base = g_bitmap_limit = 0; return; }
    // Determine bitmap bounds based on usable regions under a reasonable limit (4GB)
    uint64_t reasonable_limit = 4ULL * 1024 * 1024 * 1024; // 4GB
    // Find minimum start and maximum end within the reasonable limit
    uint64_t lo = (uint64_t)-1;
    uint64_t hi = 0;
    for (uint32_t i = 0; i < g_region_count; ++i) {
        uint64_t base = g_regions[i].base;
        uint64_t end = base + g_regions[i].len;
        // Skip regions that start beyond the reasonable limit
        if (base >= reasonable_limit) continue;
        // Clip region end to reasonable limit
        if (end > reasonable_limit) end = reasonable_limit;
        if (base < lo) lo = base;
        if (end > hi) hi = end;
    }
    // If no usable regions within limits, disable bitmap
    if (hi <= lo) { g_bitmap_base = g_bitmap_limit = 0; return; }
    g_bitmap_base = lo;
    g_bitmap_limit = hi;
    // Cap to storage capacity
    uint64_t max_cover = (uint64_t)PMM_MAX_FRAMES * PMM_FRAME_SIZE;
    if (g_bitmap_limit - g_bitmap_base > max_cover) {
        g_bitmap_limit = g_bitmap_base + max_cover;
    }
    g_bitmap = g_bitmap_storage;
    // Clear bitmap
    uint64_t frames = (g_bitmap_limit - g_bitmap_base) / PMM_FRAME_SIZE;
    for (uint64_t i = 0; i < (frames + 7)/8; ++i) g_bitmap[i] = 0;
}

static inline uint64_t addr_to_index(uint64_t paddr) {
    return (paddr - g_bitmap_base) / PMM_FRAME_SIZE;
}

static void mark_range(uint64_t paddr, uint64_t size, int used) {
    if (size == 0) return;
    uint64_t s = align_down(paddr, PMM_FRAME_SIZE);
    uint64_t e = align_up(paddr + size, PMM_FRAME_SIZE);
    if (s < g_bitmap_base) s = g_bitmap_base;
    if (e > g_bitmap_limit) e = g_bitmap_limit;
    if (e <= s) return;
    uint64_t i0 = addr_to_index(s);
    uint64_t i1 = addr_to_index(e);
    for (uint64_t i = i0; i < i1; ++i) {
        uint64_t byte = i >> 3, bit = i & 7;
        if (used) g_bitmap[byte] |= (1u << bit); else g_bitmap[byte] &= ~(1u << bit);
    }
}

static int test_frame(uint64_t idx) {
    uint64_t byte = idx >> 3, bit = idx & 7;
    return (g_bitmap[byte] >> bit) & 1u;
}

static void set_frame(uint64_t idx) {
    uint64_t byte = idx >> 3, bit = idx & 7;
    g_bitmap[byte] |= (1u << bit);
}

static void clear_frame(uint64_t idx) {
    uint64_t byte = idx >> 3, bit = idx & 7;
    g_bitmap[byte] &= ~(1u << bit);
}

void pmm_init(void* info, int from_uefi) {
    g_region_count = 0;
    g_total_phys = g_total_usable = g_free = 0;
    g_bitmap_base = g_bitmap_limit = 0;
    g_bitmap = g_bitmap_storage;
    if (info) parse_mb2(info, from_uefi);
    build_bitmap_bounds();

    // Initially mark all frames in bitmap range as used
    mark_range(g_bitmap_base, g_bitmap_limit - g_bitmap_base, 1);
    
    // Then mark usable regions as free, but only within bitmap bounds
    uint64_t usable_in_range = 0;
    for (uint32_t i = 0; i < g_region_count; ++i) {
        uint64_t reg_start = g_regions[i].base;
        uint64_t reg_end = g_regions[i].base + g_regions[i].len;
        
        // Clip region to bitmap bounds
        if (reg_start < g_bitmap_base) reg_start = g_bitmap_base;
        if (reg_end > g_bitmap_limit) reg_end = g_bitmap_limit;
        
        if (reg_end > reg_start) {
            mark_range(reg_start, reg_end - reg_start, 0);
            usable_in_range += (reg_end - reg_start);
        }
    }
    g_free = usable_in_range;

    // Reserve frame 0 to avoid allocating a null page
    pmm_reserve(g_bitmap_base, PMM_FRAME_SIZE);

    // Reserve low memory (<1MiB) to avoid allocating BIOS structures
    pmm_reserve(0, 0x100000);
}

uint64_t pmm_total_bytes(void) { return g_total_usable; }
uint64_t pmm_free_bytes(void) { return g_free; }
uint64_t pmm_total_physical_bytes(void) { return g_total_phys; }

// Reserve a physical range (e.g., kernel image, loader page tables, etc.)
void pmm_reserve(uint64_t paddr, uint64_t size) {
    uint64_t before = g_free;
    // Walk frames and mark; adjust g_free conservatively
    uint64_t s = align_down(paddr, PMM_FRAME_SIZE);
    uint64_t e = align_up(paddr + size, PMM_FRAME_SIZE);
    if (s < g_bitmap_base) s = g_bitmap_base;
    if (e > g_bitmap_limit) e = g_bitmap_limit;
    for (uint64_t a = s; a < e; a += PMM_FRAME_SIZE) {
        uint64_t idx = addr_to_index(a);
        if (!test_frame(idx)) { // was free
            set_frame(idx);
            g_free -= PMM_FRAME_SIZE;
        }
    }
    (void)before;
}

uint64_t pmm_alloc_frames(size_t count) {
    if (count == 0) return 0;
    uint64_t need = (uint64_t)count * PMM_FRAME_SIZE;
    if (g_free < need) return 0;
    uint64_t frames = (g_bitmap_limit - g_bitmap_base) / PMM_FRAME_SIZE;
    uint64_t run = 0, run_start = 0;
    for (uint64_t i = 0; i < frames; ++i) {
        if (!test_frame(i)) {
            if (run == 0) run_start = i;
            run++;
            if (run >= count) {
                // allocate
                for (uint64_t j = 0; j < count; ++j) set_frame(run_start + j);
                g_free -= need;
                return g_bitmap_base + run_start * PMM_FRAME_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

void pmm_free_frames(uint64_t paddr, size_t count) {
    if (count == 0 || paddr < g_bitmap_base || paddr >= g_bitmap_limit) return;
    uint64_t idx = addr_to_index(paddr);
    for (size_t j = 0; j < count; ++j) {
        if (idx + j >= (g_bitmap_limit - g_bitmap_base) / PMM_FRAME_SIZE) break;
        if (test_frame(idx + j)) {
            clear_frame(idx + j);
            g_free += PMM_FRAME_SIZE;
        }
    }
}

uint64_t pmm_alloc_frames_below(size_t count, uint64_t max_phys_exclusive) {
    if (count == 0) return 0;
    uint64_t need = (uint64_t)count * PMM_FRAME_SIZE;
    if (g_free < need) return 0;
    uint64_t frames = (g_bitmap_limit - g_bitmap_base) / PMM_FRAME_SIZE;
    uint64_t limit_idx = (max_phys_exclusive > g_bitmap_base)
        ? ((max_phys_exclusive - g_bitmap_base) / PMM_FRAME_SIZE)
        : 0;
    if (limit_idx > frames) limit_idx = frames;
    uint64_t run = 0, run_start = 0;
    for (uint64_t i = 0; i < limit_idx; ++i) {
        if (!test_frame(i)) {
            if (run == 0) run_start = i;
            run++;
            if (run >= count) {
                for (uint64_t j = 0; j < count; ++j) set_frame(run_start + j);
                g_free -= need;
                return g_bitmap_base + run_start * PMM_FRAME_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}
