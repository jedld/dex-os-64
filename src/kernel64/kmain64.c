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

void kmain64(void* mb_info) {
    console_init();
    uint64_t mb_addr = (uint64_t)mb_info;

    print_banner_and_info(mb_addr);
menu_loop:
    console_write("Menu:\n");
    console_write("  [Q]uick memtest (16 MiB)\n");
    console_write("  [R]ange memtest (enter start and size)\n");
    console_write("  [L] Clear screen\n");
    console_write("  [C]ontinue\n");
    console_write("Select: ");
    for (;;) {
        int ch = input_getc();
        if (ch >= 'a' && ch <= 'z') ch -= 32; // upper
        console_putc((char)ch);
        console_write("\n");
        if (ch == 'Q') {
            uint64_t errs = memtest_quick();
            console_write(errs ? "Result: FAIL\n" : "Result: PASS\n");
            console_write("Done. Press any key...\n");
            (void)input_getc();
            print_banner_and_info(mb_addr);
            goto menu_loop;
        } else if (ch == 'R') {
            char buf[64];
            console_write("Start phys (hex, e.g., 1000000): ");
            uint64_t n = input_readline(buf, sizeof(buf));
            buf[n] = '\0';
            // parse hex
            uint64_t start = 0; const char* s = buf;
            while (*s == ' ' || *s == '\t') ++s;
            while (*s) {
                char c = *s++;
                uint8_t v;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') v = 10 + (c - 'A');
                else break;
                start = (start << 4) | v;
            }
            console_write("Length (hex bytes): ");
            n = input_readline(buf, sizeof(buf)); buf[n] = '\0';
            uint64_t len = 0; s = buf;
            while (*s == ' ' || *s == '\t') ++s;
            while (*s) {
                char c = *s++;
                uint8_t v;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') v = 10 + (c - 'A');
                else break;
                len = (len << 4) | v;
            }
            uint64_t errs = memtest_run(start, len, 1);
            console_write(errs ? "Result: FAIL\n" : "Result: PASS\n");
            console_write("Done. Press any key...\n");
            (void)input_getc();
            print_banner_and_info(mb_addr);
            goto menu_loop;
        } else if (ch == 'L') {
            print_banner_and_info(mb_addr);
            goto menu_loop;
        } else if (ch == 'C' || ch == '\n' || ch == '\r' || ch == ' ') {
            // Continue boot flow. For now, just stop updating menu.
            return;
        } else {
            console_write("Unknown option. Try again: ");
        }
    }
}
