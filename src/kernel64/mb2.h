#pragma once
#include <stdint.h>

// Minimal Multiboot2 tag structures for memory map
#define MB2_TAG_END            0
#define MB2_TAG_CMDLINE        1
#define MB2_TAG_BOOT_LOADER    2
#define MB2_TAG_MODULE         3
#define MB2_TAG_MMAP           6
#define MB2_TAG_BASIC_MEMINFO  4
#define MB2_TAG_EFI32          7
#define MB2_TAG_EFI64          8
#define MB2_TAG_EFI_MMAP       12

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

// EFI memory map tag layout (type=12) per Multiboot2 spec
#pragma pack(push,1)
typedef struct {
    mb2_tag tag;           // type=12, size = total size of this tag
    uint32_t desc_size;    // size of each EFI memory descriptor
    uint32_t desc_version; // version
    // followed by desc[] blob, aligned so that overall tag is 8-byte aligned
} mb2_tag_efi_mmap_hdr;

// Minimal subset of EFI_MEMORY_DESCRIPTOR (UEFI spec)
typedef struct {
    uint32_t Type;         // 7 = EfiConventionalMemory
    uint32_t Pad;          // align
    uint64_t PhysicalStart;
    uint64_t VirtualStart;
    uint64_t NumberOfPages; // 4KiB pages
    uint64_t Attribute;
} efi_mem_desc;
#pragma pack(pop)

// Basic memory info (type=4)
#pragma pack(push,1)
typedef struct {
    mb2_tag tag;       // type=4, size=16
    uint32_t mem_lower; // in KiB
    uint32_t mem_upper; // in KiB (starting at 1MiB)
} mb2_tag_basic_meminfo;
#pragma pack(pop)

static inline const mb2_tag* mb2_first_tag(uint64_t mb2_addr) {
    return (const mb2_tag*)(mb2_addr + 8); // after total_size + reserved
}

static inline const mb2_tag* mb2_next_tag(const mb2_tag* t) {
    uint64_t addr = (uint64_t)t;
    uint64_t next = (addr + t->size + 7) & ~7ULL;
    return (const mb2_tag*)next;
}

// Iterate entries inside an EFI mmap tag
static inline const efi_mem_desc* mb2_efi_first_desc(const mb2_tag_efi_mmap_hdr* h) {
    return (const efi_mem_desc*)((const uint8_t*)h + sizeof(*h));
}
static inline uint32_t mb2_efi_desc_count(const mb2_tag_efi_mmap_hdr* h) {
    if (h->desc_size == 0) return 0;
    uint32_t payload = h->tag.size - sizeof(*h);
    return payload / h->desc_size;
}
