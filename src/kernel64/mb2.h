#pragma once
#include <stdint.h>

// Minimal Multiboot2 tag structures for memory map
#define MB2_TAG_END            0
#define MB2_TAG_CMDLINE        1
#define MB2_TAG_BOOT_LOADER    2
#define MB2_TAG_MODULE         3
#define MB2_TAG_MMAP           6

#pragma pack(push,1)
typedef struct { uint32_t type, size; } mb2_tag;

typedef struct { mb2_tag tag; /* followed by entries */ } mb2_tag_mmap_hdr;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;   // 1=available RAM
    uint32_t reserved;
} mb2_mmap_entry;
#pragma pack(pop)

static inline const mb2_tag* mb2_first_tag(uint64_t mb2_addr) {
    return (const mb2_tag*)(mb2_addr + 8); // after total_size + reserved
}

static inline const mb2_tag* mb2_next_tag(const mb2_tag* t) {
    uint64_t addr = (uint64_t)t;
    uint64_t next = (addr + t->size + 7) & ~7ULL;
    return (const mb2_tag*)next;
}
