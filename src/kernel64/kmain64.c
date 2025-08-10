// 64-bit kernel main: VGA console output + CPU and memory stats
#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "io.h"
#include "cpuid.h"
#include "mb2.h"
#include "serial.h"
#include "input.h"
#include "memtest.h"
// New subsystems
#include "../kernel/mm/pmm.h"
#include "../kernel/mm/vmm.h"
#include "../kernel/mm/kmalloc.h"
#include "sched/sched.h"
// Devices and shell
#include "dev/device.h"
#include "vfs/vfs.h"
#include "fs/exfat.h"
#include "usb/usb.h"
void exfat_register(void);
void devfs_register(void);
int ramdisk_create(const char* name, uint64_t bytes);
void display_console_register(void);
void kb_ps2_register(void);
void shell_main(void*);

static void s_puts(const char* s){
    while (*s) { serial_putc(*s++); }
    serial_putc('\n');
}

static void s_put_hex64(uint64_t v){
    static const char lut[16] = "0123456789ABCDEF";
    serial_putc('0'); serial_putc('x');
    for (int i=15;i>=0;--i){ uint8_t n=(v>>(i*4))&0xF; serial_putc(lut[n]); }
}

// Symbols from linker script to identify kernel image range in memory
extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

static inline uint64_t read_cr3_phys(void) {
    uint64_t v; __asm__ volatile ("mov %%cr3,%0" : "=r"(v)); return v;
}

static void print_vendor(void) {
    cpuid_regs r0 = cpuid(0, 0);
    char vendor[13];
    *(uint32_t*)&vendor[0] = r0.ebx;
    *(uint32_t*)&vendor[4] = r0.edx;
    *(uint32_t*)&vendor[8] = r0.ecx;
    vendor[12] = '\0';
    console_write("CPU vendor: "); console_write(vendor); console_write("\n");
}

static void print_brand(void) {
    char brand[49];
    cpuid_regs r;
    r = cpuid(0x80000002, 0); *(uint32_t*)&brand[0] = r.eax; *(uint32_t*)&brand[4] = r.ebx; *(uint32_t*)&brand[8] = r.ecx; *(uint32_t*)&brand[12] = r.edx;
    r = cpuid(0x80000003, 0); *(uint32_t*)&brand[16] = r.eax; *(uint32_t*)&brand[20] = r.ebx; *(uint32_t*)&brand[24] = r.ecx; *(uint32_t*)&brand[28] = r.edx;
    r = cpuid(0x80000004, 0); *(uint32_t*)&brand[32] = r.eax; *(uint32_t*)&brand[36] = r.ebx; *(uint32_t*)&brand[40] = r.ecx; *(uint32_t*)&brand[44] = r.edx;
    brand[48] = '\0';
    console_write("CPU brand:  "); console_write(brand); console_write("\n");
}

static void print_features(void) {
    cpuid_regs r1 = cpuid(1, 0);
    console_write("Features ECX="); console_write_hex64(r1.ecx); console_write(" EDX="); console_write_hex64(r1.edx); console_write("\n");
    s_puts("[k64] cpuid(1,0) raw:");
    s_put_hex64(((uint64_t)r1.edx<<32)|r1.ecx); serial_putc('\n');
}

static void print_memory_map(uint64_t mb2_addr) {
    uint64_t total_ram = 0;
    uint64_t total_reserved = 0;
    console_write("Memory map:\n");
    // Prefer EFI memory map if provided; else fall back to legacy MB2 mmap
    const mb2_tag* t = mb2_first_tag(mb2_addr);
    const mb2_tag_efi_mmap_hdr* efi = NULL;
    const mb2_tag_mmap_hdr* mmap = NULL;
    while (t && t->type != MB2_TAG_END) {
        if (t->type == MB2_TAG_EFI_MMAP && !efi) efi = (const mb2_tag_efi_mmap_hdr*)t;
        if (t->type == MB2_TAG_MMAP && !mmap) mmap = (const mb2_tag_mmap_hdr*)t;
        t = mb2_next_tag(t);
    }
    if (efi) {
        uint32_t count = mb2_efi_desc_count(efi);
        const uint8_t* base = (const uint8_t*)mb2_efi_first_desc(efi);
        for (uint32_t i = 0; i < count; ++i) {
            const efi_mem_desc* d = (const efi_mem_desc*)(base + i * efi->desc_size);
            uint64_t bytes = d->NumberOfPages * 4096ULL;
            int usable = (d->Type == 7 /*EfiConventionalMemory*/);
            if (usable) total_ram += bytes; else total_reserved += bytes;
            console_write("  ");
            console_write_hex64(d->PhysicalStart);
            console_write(" + ");
            console_write_hex64(bytes);
            console_write(usable ? "  [USABLE]\n" : "  [RESV]\n");
        }
    } else if (mmap) {
        const uint8_t* p = (const uint8_t*)mmap;
        uint32_t entry_size = *(const uint32_t*)(p + 8);
        uint32_t offset = 16;
        while (offset + entry_size <= mmap->tag.size) {
            const mb2_mmap_entry* e = (const mb2_mmap_entry*)(p + offset);
            if (e->type == 1) total_ram += e->length; else total_reserved += e->length;
            console_write("  ");
            console_write_hex64(e->base_addr);
            console_write(" + ");
            console_write_hex64(e->length);
            console_write(e->type == 1 ? "  [USABLE]\n" : "  [RESV]\n");
            offset += entry_size;
        }
    }

    // Fallback to basic meminfo if totals are still zero
    if (total_ram == 0 && total_reserved == 0) {
        const mb2_tag* tt = mb2_first_tag(mb2_addr);
        while (tt && tt->type != MB2_TAG_END) {
            if (tt->type == MB2_TAG_BASIC_MEMINFO) {
                const mb2_tag_basic_meminfo* bi = (const mb2_tag_basic_meminfo*)tt;
                uint64_t lower = (uint64_t)bi->mem_lower * 1024ULL;
                uint64_t upper = (uint64_t)bi->mem_upper * 1024ULL;
                total_ram = lower + upper;
                console_write("  (basic meminfo used)\n");
                break;
            }
            tt = mb2_next_tag(tt);
        }
    }
    console_write("Total RAM: "); console_write_hex64(total_ram); console_write(" bytes\n");
    console_write("Reserved:  "); console_write_hex64(total_reserved); console_write(" bytes\n");
}

static void print_banner_and_info(uint64_t mb_addr) {
    console_clear();
    console_set_color(0x0F, 0x00);
    console_write("dex-os-64 (x86_64)\n\n");
    print_vendor();
    print_brand();
    print_features();
    console_write("\n");
    print_memory_map(mb_addr);
    console_write("\n");
}

// Demo worker threads for scheduler
static void workerA(void* _) { (void)_; for (int i=0;i<50;++i){ console_putc('.'); sched_yield(); } }
static void workerB(void* _) { (void)_; for (int i=0;i<50;++i){ console_putc('-'); sched_yield(); } }

// SMP test worker and args
typedef struct { int id; } smp_arg_t;
static smp_arg_t smp_args[8];
static void smp_worker(void* p) {
    int id = ((smp_arg_t*)p)->id;
    for (int k = 0; k < 40; ++k) { console_putc('0' + (id % 10)); sched_yield(); }
}

void kmain64(void* mb_info) {
    // Early serial breadcrumb
    serial_init();
    s_puts("[k64] entry");
    console_init_from_mb2((uint64_t)mb_info);
    s_puts("[k64] console_init");
    uint64_t mb_addr = (uint64_t)mb_info;

    // Sanity self-test to validate hex table and printing path
    console_write("Selftest: ");
    console_write_hex64(0x0123456789ABCDEFULL);
    console_write("\n");
    console_write("Digits: 0123456789ABCDEF\n");
    s_puts("[k64] ser selftest hex:"); s_put_hex64(0x0123456789ABCDEFULL); serial_putc('\n');

    print_banner_and_info(mb_addr);
    s_puts("[k64] printed banner");
    // Capture loader paging structures currently in use so we can reserve
    // them before building our own page tables. We assume identity mapping
    // is active (set by the loader), so physical addresses are directly
    // dereferenceable.
    uint64_t loader_pml4 = read_cr3_phys();
    uint64_t* pml4 = (uint64_t*)(uintptr_t)loader_pml4;
    uint64_t loader_pdpt = (pml4[0] & ~0xFFFULL);
    uint64_t* pdpt = (uint64_t*)(uintptr_t)loader_pdpt;
    uint64_t loader_pd[4] = {
        (pdpt[0] & ~0xFFFULL),
        (pdpt[1] & ~0xFFFULL),
        (pdpt[2] & ~0xFFFULL),
        (pdpt[3] & ~0xFFFULL)
    };

    // Bring up physical memory manager and reserve critical ranges before any allocation
    pmm_init((void*)mb_addr, 0);
    // Reserve the Multiboot2 info area itself so PMM won't reuse it
    if (mb_addr) {
        uint32_t mb_total = *(volatile uint32_t*)(uintptr_t)mb_addr;
        if (mb_total > 0 && mb_total < (16U<<20)) { // sanity: <16MiB
            pmm_reserve(mb_addr, (uint64_t)mb_total);
        }
    }
    // Reserve the kernel image (text+data+bss+stack)
    uint64_t kstart = (uint64_t)(uintptr_t)&__kernel_start;
    uint64_t kend   = (uint64_t)(uintptr_t)&__kernel_end;
    if (kend > kstart) {
        pmm_reserve(kstart, kend - kstart);
    }
    // Reserve the loader's paging structures still in use until we switch CR3
    pmm_reserve(loader_pml4, 4096);
    pmm_reserve(loader_pdpt, 4096);
    for (int i = 0; i < 4; ++i) if (loader_pd[i]) pmm_reserve(loader_pd[i], 4096);
    s_puts("[k64] pmm_init");
    vmm_init_identity();
    s_puts("[k64] vmm_init_identity");
    // Early heap: 256 KiB static region
    static uint8_t early_heap[256 * 1024] __attribute__((aligned(16)));
    kmalloc_init(early_heap, sizeof(early_heap));
    s_puts("[k64] kmalloc_init");
    console_write("PMM/VMM initialized. Free: "); console_write_hex64(pmm_free_bytes()); console_write(" bytes\n\n");
    // Register basic devices
    display_console_register();
    kb_ps2_register();
    // Probe PCI/USB controllers (skeleton)
    usb_init();
    // Register filesystems
    exfat_register(); s_puts("[k64] exfat_register");
    devfs_register(); s_puts("[k64] devfs_register");

    // Auto-mount devfs and a RAM-backed root filesystem (exFAT on ram0)
    console_write("Setting up root filesystem...\n");
    int rc;
    // 1) devfs at mount name 'dev'
    rc = vfs_mount("devfs", "dev", "");
    if (rc==0) { s_puts("[k64] mounted devfs as 'dev'"); } else { s_puts("[k64] devfs mount FAILED"); }
    // 2) Create an 8 MiB RAM disk named 'ram0'
    uint64_t ram_bytes = 8ULL * 1024 * 1024;
    if (ramdisk_create("ram0", ram_bytes)==0) { s_puts("[k64] ramdisk ram0 created"); }
    else { s_puts("[k64] ramdisk ram0 creation FAILED"); }
    // Discover partitions on available block devices (MBR only for now)
    extern void block_scan_partitions(void);
    s_puts("[k64] scan partitions enter");
    block_scan_partitions();
    s_puts("[k64] scan partitions exit");
    // 3) Format as exFAT
    s_puts("[k64] exfat mkfs enter");
    if (exfat_format_device("ram0", "" )==0) { s_puts("[k64] exfat mkfs OK on ram0"); }
    else { s_puts("[k64] exfat mkfs FAILED on ram0"); }
    // 4) Mount exfat as 'root'
    s_puts("[k64] mount exfat root enter");
    rc = vfs_mount("exfat", "root", "ram0");
    if (rc==0) { s_puts("[k64] mounted exfat 'root' on ram0"); }
    else { s_puts("[k64] mount exfat root FAILED"); }
    console_write("Mounts after setup:\n");
    vfs_list_mounts();
    console_write("\n");

    // Start shell by default
    s_puts("[k64] start shell");
    console_write("Starting shell...\n");
    sched_create(shell_main, NULL);
    sched_start();
    for(;;){ __asm__ volatile ("hlt"); }
}
