// 64-bit kernel main: VGA console output + CPU and memory stats
#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "io.h"
#include "cpuid.h"
#include "mb2.h"
#include "serial.h"

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
    console_write("Features ECX=0x"); console_write_hex64(r1.ecx); console_write(" EDX=0x"); console_write_hex64(r1.edx); console_write("\n");
}

static void print_memory_map(uint64_t mb2_addr) {
    uint64_t total_ram = 0;
    uint64_t total_reserved = 0;
    console_write("Memory map:\n");
    const mb2_tag* t = mb2_first_tag(mb2_addr);
    while (t && t->type != MB2_TAG_END) {
        if (t->type == MB2_TAG_MMAP) {
            const mb2_tag_mmap_hdr* mh = (const mb2_tag_mmap_hdr*)t;
            // Entries start after header; entry size is at offset 8 after tag header in full spec,
            // but Multiboot2 standard layout: [type,size,u32 entry_size,u32 entry_version, entries...]
            const uint8_t* p = (const uint8_t*)t;
            uint32_t entry_size = *(const uint32_t*)(p + 8);
            uint32_t offset = 16;
            while (offset + entry_size <= t->size) {
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
        t = mb2_next_tag(t);
    }
    console_write("Total RAM: "); console_write_hex64(total_ram); console_write(" bytes\n");
    console_write("Reserved:  "); console_write_hex64(total_reserved); console_write(" bytes\n");
}

void kmain64(void* mb_info) {
    // Debug marker - reached kmain64
    serial_putc('X');
    
    console_init();
    
    // Debug marker - console initialized
    serial_putc('Y');

    console_set_color(0x0F, 0x00); // white on black
    console_write("dex-os-64 (x86_64)\n\n");

    // Debug marker - about to print CPU info
    serial_putc('2');

    print_vendor();
    print_brand();
    print_features();
    console_write("\n");
    print_memory_map((uint64_t)mb_info);
    console_write("\nDone.\n");
    console_write("Press any key to continue (PS/2 or serial) ...\n");

    // Debug marker - about to wait for keyboard
    serial_putc('3');

    // Wait for a keypress from either PS/2 keyboard or serial console
    // Clear any pending PS/2 scancodes
    while (inb(0x64) & 1) { (void)inb(0x60); }
    for (;;) {
        // PS/2 key?
        if (inb(0x64) & 1) { (void)inb(0x60); break; }
        // Serial key?
        int ch = serial_try_getc();
        if (ch >= 0) { break; }
    }

    // Debug marker - key pressed, ending
    serial_putc('4');
}
