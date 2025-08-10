#pragma once
#include <stdint.h>

// Minimal Multiboot2 tag structures for memory map
#define MB2_TAG_END            0
#define MB2_TAG_CMDLINE        1
#define MB2_TAG_BOOT_LOADER    2
#define MB2_TAG_MODULE         3
#define MB2_TAG_MMAP           6
#define MB2_TAG_BASIC_MEMINFO  4
#define MB2_TAG_EFI32          11   // EFI 32-bit system table pointer (per spec)
#define MB2_TAG_EFI64          12   // EFI 64-bit system table pointer (per spec)
#define MB2_TAG_EFI_MMAP       17   // EFI memory map (per spec)
// Additional commonly used tags
#define MB2_TAG_FRAMEBUFFER     8   // Framebuffer info (RGB/EGA text)
#define MB2_TAG_VBE             7   // VBE info

#pragma pack(push,1)
typedef struct { uint32_t type, size; } mb2_tag;
// Multiboot2 Module tag (type=3) — per spec, addresses are 32-bit physical
typedef struct {
    mb2_tag tag;       // type=3, size
    uint32_t mod_start; // physical start (32-bit)
    uint32_t mod_end;   // physical end (exclusive, 32-bit)
    // followed by zero-terminated string (module cmdline/name)
} mb2_tag_module;

static inline uint64_t mb2_mod_start(const mb2_tag_module* m){
    return (uint64_t)m->mod_start;
}
static inline uint64_t mb2_mod_end(const mb2_tag_module* m){
    return (uint64_t)m->mod_end;
}
static inline const char* mb2_mod_string(const mb2_tag_module* m){
    const char* s = (const char*)(m+1);
    return s;
}


// Multiboot2 Memory Map tag (type=6)
typedef struct {
    mb2_tag tag;        // type=6, size=total size of this tag including header
    uint32_t entry_size;    // size of each mb2_mmap_entry
    uint32_t entry_version; // version
    // followed by entries[]
} mb2_tag_mmap_hdr;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;   // 1=available RAM
    uint32_t reserved;
} mb2_mmap_entry;
#pragma pack(pop)

// EFI memory map tag layout (type=17) per Multiboot2 spec
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

// Framebuffer info (type=8)
#pragma pack(push,1)
typedef struct {
    uint32_t type;              // = 8
    uint32_t size;
    uint64_t framebuffer_addr;  // physical
    uint32_t framebuffer_pitch; // bytes per line (pixels for RGB, chars*2 for EGA text)
    uint32_t framebuffer_width; // pixels for RGB, chars for EGA text
    uint32_t framebuffer_height;// pixels for RGB, chars for EGA text
    uint8_t  framebuffer_bpp;   // bits per pixel (16 for EGA text)
    uint8_t  framebuffer_type;  // 0=indexed,1=RGB,2=EGA text
    uint16_t reserved;
    // Followed by either palette (indexed) or RGB mask info (type=1)
} mb2_tag_framebuffer_common;

typedef struct {
    mb2_tag_framebuffer_common common;
    // For RGB type=1
    struct {
        uint8_t red_field_position;
        uint8_t red_mask_size;
        uint8_t green_field_position;
        uint8_t green_mask_size;
        uint8_t blue_field_position;
        uint8_t blue_mask_size;
    } rgb_info;
} mb2_tag_framebuffer;
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
