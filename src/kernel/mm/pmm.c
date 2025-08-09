// pmm.c — Physical Memory Manager skeleton
// Parses Multiboot2 memory map to compute total and usable memory.
// Next: build a frame bitmap allocator and subtract kernel/boot reserved areas.
// Minimal Multiboot2 memory map parser to compute total and usable memory.
#include "pmm.h"
#include <stdint.h>

// Keep totals; a real implementation will build a frame bitmap.
static uint64_t g_total_phys = 0;
static uint64_t g_total_usable = 0;
static uint64_t g_free = 0;

// Multiboot2 structures (subset) to avoid external headers
struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_mmap {
    uint32_t type; // 6
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // followed by entries
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type; // 1 = available
    uint32_t zero;
};

enum { MB2_TAG_TYPE_MMAP = 6 };

static void parse_mb2(void* info) {
    uint8_t* base = (uint8_t*)info;
    uint32_t total_size = *(uint32_t*)base;
    // Skip fixed header (size, reserved)
    uint32_t off = 8;
    while (off + sizeof(struct mb2_tag) <= total_size) {
        struct mb2_tag* tag = (struct mb2_tag*)(base + off);
        if (tag->type == 0 || tag->size == 0) break;
        if (tag->type == MB2_TAG_TYPE_MMAP) {
            struct mb2_tag_mmap* mm = (struct mb2_tag_mmap*)tag;
            uint32_t entries_size = mm->size - sizeof(struct mb2_tag_mmap);
            uint8_t* ep = (uint8_t*)mm + sizeof(struct mb2_tag_mmap);
            for (uint32_t o = 0; o + mm->entry_size <= entries_size; o += mm->entry_size) {
                struct mb2_mmap_entry* e = (struct mb2_mmap_entry*)(ep + o);
                g_total_phys += e->len;
                if (e->type == 1) {
                    g_total_usable += e->len;
                }
            }
        }
        // 8-byte align next tag
        off += (tag->size + 7) & ~7U;
    }
    // Initial free equals usable for now (not subtracting kernel/boot yet)
    g_free = g_total_usable;
}

void pmm_init(void* info, int from_uefi) {
    (void)from_uefi;
    g_total_phys = 0;
    g_total_usable = 0;
    g_free = 0;
    if (info) parse_mb2(info);
}

uint64_t pmm_total_bytes(void) { return g_total_usable; }
uint64_t pmm_free_bytes(void) { return g_free; }
uint64_t pmm_total_physical_bytes(void) { return g_total_phys; }

uint64_t pmm_alloc_frames(size_t count) {
    uint64_t bytes = (uint64_t)count * PMM_FRAME_SIZE;
    if (g_free < bytes) return 0;
    g_free -= bytes;
    // TODO: return actual physical frame start from bitmap
    return 0x100000; // placeholder
}

void pmm_free_frames(uint64_t paddr, size_t count) {
    (void)paddr;
    uint64_t bytes = (uint64_t)count * PMM_FRAME_SIZE;
    g_free += bytes;
}
