// pmm.c — Physical Memory Manager using a simple bitmap over available regions
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

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

// Multiboot2 minimal structures to parse mmap
struct mb2_tag { uint32_t type, size; };
struct mb2_tag_mmap { uint32_t type, size, entry_size, entry_version; };
struct mb2_mmap_entry { uint64_t addr, len; uint32_t type, zero; };
enum { MB2_TAG_TYPE_MMAP = 6 };

static inline uint64_t align_down(uint64_t x, uint64_t a) { return x & ~(a-1); }
static inline uint64_t align_up(uint64_t x, uint64_t a) { return (x + (a-1)) & ~(a-1); }

static void add_region(uint64_t base, uint64_t len) {
    if (len == 0 || g_region_count >= MAX_REGIONS) return;
    g_regions[g_region_count++] = (region_t){ base, len };
}

static void parse_mb2(void* info) {
    uint8_t* base = (uint8_t*)info;
    uint32_t total_size = *(uint32_t*)base;
    uint32_t off = 8; // skip header
    while (off + sizeof(struct mb2_tag) <= total_size) {
        struct mb2_tag* tag = (struct mb2_tag*)(base + off);
        if (tag->type == 0 || tag->size < sizeof(struct mb2_tag)) break;
        if (tag->type == MB2_TAG_TYPE_MMAP) {
            struct mb2_tag_mmap* mm = (struct mb2_tag_mmap*)tag;
            uint32_t esize = mm->entry_size;
            uint32_t entries_size = mm->size - sizeof(struct mb2_tag_mmap);
            uint8_t* ep = (uint8_t*)mm + sizeof(struct mb2_tag_mmap);
            for (uint32_t o = 0; o + esize <= entries_size; o += esize) {
                struct mb2_mmap_entry* e = (struct mb2_mmap_entry*)(ep + o);
                g_total_phys += e->len;
                if (e->type == 1 /*available*/) {
                    uint64_t s = align_up(e->addr, PMM_FRAME_SIZE);
                    uint64_t end = align_down(e->addr + e->len, PMM_FRAME_SIZE);
                    if (end > s) {
                        add_region(s, end - s);
                        g_total_usable += (end - s);
                    }
                }
            }
        }
        off += (tag->size + 7) & ~7U; // align 8
    }
}

static void build_bitmap_bounds(void) {
    if (g_region_count == 0) { g_bitmap_base = g_bitmap_limit = 0; return; }
    uint64_t lo = g_regions[0].base, hi = g_regions[0].base + g_regions[0].len;
    for (uint32_t i = 1; i < g_region_count; ++i) {
        if (g_regions[i].base < lo) lo = g_regions[i].base;
        uint64_t t = g_regions[i].base + g_regions[i].len;
        if (t > hi) hi = t;
    }
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
    (void)from_uefi;
    g_region_count = 0;
    g_total_phys = g_total_usable = g_free = 0;
    g_bitmap_base = g_bitmap_limit = 0;
    g_bitmap = g_bitmap_storage;
    if (info) parse_mb2(info);
    build_bitmap_bounds();

    // Initially mark all usable frames as free; all outside regions are considered used.
    // Start with all used, then clear usable.
    mark_range(g_bitmap_base, g_bitmap_limit - g_bitmap_base, 1);
    for (uint32_t i = 0; i < g_region_count; ++i) {
        mark_range(g_regions[i].base, g_regions[i].len, 0);
    }
    g_free = g_total_usable;
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
