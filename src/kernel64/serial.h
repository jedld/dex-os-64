#pragma once
#include <stdint.h>
#include "io.h"

#define COM1_BASE 0x3F8
#define COM_LSR   (COM1_BASE + 5)
#define COM_LSR_THRE 0x20
// Data Ready bit in LSR indicates received byte is available
#define COM_LSR_DR   0x01

// Initialize serial port (COM1) for output
void serial_init(void);

// Wait until transmitter holding register is empty
void serial_wait_tx(void);

// Write a character to serial port (blocking)
void serial_putc(char c);

// Non-blocking read: returns byte [0..255] if available, otherwise -1
// Read a character from serial port if available, -1 otherwise
int serial_try_getc(void);
