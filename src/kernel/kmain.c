#include <stdint.h>

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

void kmain(uint32_t mb_info) {
    (void)mb_info;
    serial_init();
    serial_write("Hello from Multiboot2 kernel!\n");
    for (;;) { __asm__ volatile ("hlt"); }
}
