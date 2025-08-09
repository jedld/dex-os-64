#pragma once
#include <stdint.h>

// Simple unified input: PS/2 keyboard and serial
// Non-blocking getc: returns byte [0..255] or -1 if no input
int input_try_getc(void);

// Blocking getc: waits for a key and returns it
int input_getc(void);

// Read a line (up to max_len-1), echoing characters; returns length (no '\0' stored)
uint64_t input_readline(char* buf, uint64_t max_len);
