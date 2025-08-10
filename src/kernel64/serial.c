// Serial port implementation
#include "serial.h"
#include "io.h"
#include <stdint.h>

void serial_init(void) {
    // Disable interrupts
    outb(COM1_BASE + 1, 0x00);
    // Enable DLAB to set baud divisor
    outb(COM1_BASE + 3, 0x80);
    // Divisor 1 -> 115200 bps
    outb(COM1_BASE + 0, 0x01);
    outb(COM1_BASE + 1, 0x00);
    // 8 bits, no parity, one stop bit
    outb(COM1_BASE + 3, 0x03);
    // Enable FIFO, clear them, 14-byte threshold
    outb(COM1_BASE + 2, 0xC7);
    // IRQs enabled, RTS/DSR set
    outb(COM1_BASE + 4, 0x0B);
}

void serial_wait_tx(void) {
    while ((inb(COM_LSR) & COM_LSR_THRE) == 0) { }
}

void serial_putc(char c) {
    serial_wait_tx();
    outb(COM1_BASE, (uint8_t)c);
}

int serial_try_getc(void) {
    if ((inb(COM_LSR) & COM_LSR_DR) != 0) {
        return (int)inb(COM1_BASE);
    }
    return -1;
}
