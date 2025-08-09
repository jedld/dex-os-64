#include "console.h"
#include "io.h"
#include <stddef.h>

#define VGA_MEM ((volatile uint16_t*)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static uint8_t s_color = 0x0F; // white on black
static size_t s_row = 0, s_col = 0;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void move_cursor(void) {
    uint16_t pos = (uint16_t)(s_row * VGA_COLS + s_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void console_set_color(uint8_t fg, uint8_t bg) { s_color = (bg << 4) | (fg & 0x0F); }

void console_clear(void) {
    for (size_t r = 0; r < VGA_ROWS; ++r) {
        for (size_t c = 0; c < VGA_COLS; ++c) {
            VGA_MEM[r * VGA_COLS + c] = vga_entry(' ', s_color);
        }
    }
    s_row = s_col = 0;
    move_cursor();
}

void console_init(void) {
    console_clear();
}

void console_putc(char c) {
    if (c == '\n') {
        s_col = 0;
        if (++s_row >= VGA_ROWS) s_row = 0;
        move_cursor();
        return;
    }
    VGA_MEM[s_row * VGA_COLS + s_col] = vga_entry(c, s_color);
    if (++s_col >= VGA_COLS) { s_col = 0; if (++s_row >= VGA_ROWS) s_row = 0; }
    move_cursor();
}

void console_write(const char* s) { while (*s) console_putc(*s++); }

static const char* hex = "0123456789ABCDEF";
void console_write_hex64(uint64_t v) {
    console_write("0x");
    for (int i = 15; i >= 0; --i) {
        uint8_t nyb = (v >> (i*4)) & 0xF;
        console_putc(hex[nyb]);
    }
}

void console_write_dec(uint64_t v) {
    char buf[32]; size_t i = 0;
    if (v == 0) { console_putc('0'); return; }
    while (v > 0 && i < sizeof(buf)) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) console_putc(buf[i]);
}
