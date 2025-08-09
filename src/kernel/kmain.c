// kmain.c — kernel entry (current: Multiboot2 32-bit), serial logging, and early heap demo.
// TODO: After long-mode trampoline is added, kmain64 will become the main 64-bit entry.
#include <stdint.h>
#include <stddef.h>
#include "mm/kmalloc.h"
#include "mm/pmm.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00); // Disable all interrupts
    outb(0x3F8 + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(0x3F8 + 1, 0x00); //                  (hi byte)
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int serial_is_transmit_empty(void) {
    return (inb(0x3F8 + 5) & 0x20) != 0;
}

static void serial_write_char(char a) {
    while (serial_is_transmit_empty() == 0) { }
    outb(0x3F8, (uint8_t)a);
}

static void serial_write(const char* s) {
    while (*s) {
        if (*s == '\n') serial_write_char('\r');
        serial_write_char(*s++);
    }
}

static void print_u64_hex(uint64_t v) {
    const char* hex = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; ++i) {
        int shift = (15 - i) * 4;
        buf[2 + i] = hex[(v >> shift) & 0xF];
    }
    buf[18] = 0;
    serial_write(buf);
}

static void print_u32_dec(uint32_t x) {
    char tmp[16]; int i = 0;
    if (x == 0) { serial_write("0"); return; }
    while (x > 0 && i < (int)sizeof(tmp)) { tmp[i++] = '0' + (x % 10); x /= 10; }
    for (int j = i - 1; j >= 0; --j) serial_write((char[]){tmp[j], 0});
}

static void print_human_bytes(uint64_t bytes) {
    // Use shifts only; print approximate largest unit without 64-bit division.
    uint32_t kib = (uint32_t)(bytes >> 10);
    uint32_t mib = (uint32_t)(bytes >> 20);
    uint32_t gib = (uint32_t)(bytes >> 30);
    serial_write(" (~");
    if (gib > 0) { print_u32_dec(gib); serial_write(" GiB"); }
    else if (mib > 0) { print_u32_dec(mib); serial_write(" MiB"); }
    else { print_u32_dec(kib); serial_write(" KiB"); }
    serial_write(")");
}

void kmain(uint32_t mb_info) {
    serial_init();
    serial_write("Hello from Multiboot2 kernel!\n");
    // Initialize PMM from Multiboot2 info and print memory totals
    pmm_init((void*)(uintptr_t)mb_info, 0);
    serial_write("Total physical: "); print_u64_hex(pmm_total_physical_bytes()); print_human_bytes(pmm_total_physical_bytes()); serial_write("\n");
    serial_write("Usable (free init): "); print_u64_hex(pmm_total_bytes()); print_human_bytes(pmm_total_bytes()); serial_write("\n");
    // Init a small static heap for early allocations (64 KiB)
    static uint8_t early_heap[64 * 1024] __attribute__((aligned(16)));
    kmalloc_init(early_heap, sizeof(early_heap));
    void* a = kmalloc(24);
    void* b = kmalloc(1024);
    void* c = kmalloc(4096);
    serial_write("kmalloc test done\n");
    (void)a; (void)b; (void)c;
    for (;;) { __asm__ volatile ("hlt"); }
}
