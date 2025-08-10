#pragma once
#include <stdint.h>
#include "io.h"

#define COM1_BASE 0x3F8
#define COM_LSR   (COM1_BASE + 5)
#define COM_LSR_THRE 0x20
// Data Ready bit in LSR indicates received byte is available
#define COM_LSR_DR   0x01

static inline void serial_init(void) {
    // Disable interrupts
    outb(COM1_BASE + 1, 0x00);
    // Enable DLAB to set baud divisor
    outb(COM1_BASE + 3, 0x80);
    // Divisor 1 -> 115200 bps (QEMU default)
    outb(COM1_BASE + 0, 0x01); // DLL
    outb(COM1_BASE + 1, 0x00); // DLM
    // 8 bits, no parity, one stop bit
    outb(COM1_BASE + 3, 0x03);
    // Enable FIFO, clear them, 14-byte threshold
    outb(COM1_BASE + 2, 0xC7);
    // IRQs enabled, RTS/DSR set
    outb(COM1_BASE + 4, 0x0B);
}

static inline void serial_wait_tx(void) {
    // Wait until Transmitter Holding Register Empty
    while ((inb(COM_LSR) & COM_LSR_THRE) == 0) {
        // spin
    }
}

static inline void serial_putc(char c) {
    serial_wait_tx();
    outb(COM1_BASE, (uint8_t)c);
}

// Non-blocking read: returns byte [0..255] if available, otherwise -1
static inline int serial_try_getc(void) {
    if ((inb(COM_LSR) & COM_LSR_DR) != 0) {
        return (int)inb(COM1_BASE);
    }
    return -1;
}
