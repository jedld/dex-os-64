#pragma once
#include <stdint.h>
#include "io.h"

#define COM1_BASE 0x3F8
#define COM_LSR   (COM1_BASE + 5)
#define COM_LSR_THRE 0x20
// Data Ready bit in LSR indicates received byte is available
#define COM_LSR_DR   0x01

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
