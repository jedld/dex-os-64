// Console subsystem with multi-instance support (VGA text backend + serial mirror)
#include "console.h"
#include "serial.h"
#include "io.h"
#include "mb2.h"
#include <stdint.h>
#include <stddef.h>

// VGA text backend state
#define VGA_MEM_DEFAULT ((volatile uint16_t*)0xB8000)
#ifndef CONSOLE_COLS
#define CONSOLE_COLS 80
#endif
#define VGA_DEFAULT_COLS CONSOLE_COLS
#define VGA_DEFAULT_ROWS 25

struct Console {
    // geometry
    uint16_t cols, rows;
    // cursor and color
    uint16_t row, col;
    uint8_t color; // high nibble = bg, low nibble = fg
    // backend binding
    volatile uint16_t* vram; // mapped text VRAM (VGA 0xB8000 or EGA text framebuffer)
    uint16_t vram_pitch_chars; // chars per line (bytes per line / 2)
    // offscreen text buffer (VGA 80x25 max)
    uint16_t cells[VGA_DEFAULT_COLS * VGA_DEFAULT_ROWS];
};

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static inline void vga_move_hw_cursor(uint16_t row, uint16_t col, uint16_t cols) {
    uint16_t pos = (uint16_t)(row * cols + col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static Console s_consoles[4];
static Console* s_active = 0;

static void vga_flush(Console* c) {
    if (c != s_active) return;
    for (uint16_t r = 0; r < c->rows; ++r) {
        for (uint16_t co = 0; co < c->cols; ++co) {
            // write into bound VRAM, respecting pitch
            c->vram[r * c->vram_pitch_chars + co] = c->cells[r * c->cols + co];
        }
    }
    vga_move_hw_cursor(c->row, c->col, c->cols);
}

static void vga_clear(Console* c) {
    uint16_t cols = c->cols, rows = c->rows;
    uint16_t fill = vga_entry(' ', c->color);
    for (uint16_t r = 0; r < rows; ++r) {
        for (uint16_t co = 0; co < cols; ++co) {
            c->cells[r * cols + co] = fill;
        }
    }
    c->row = c->col = 0;
    if (c == s_active) {
        vga_flush(c);
    }
}

static void vga_scroll_up(Console* c) {
    // scroll by one line
    uint16_t cols = c->cols, rows = c->rows;
    for (uint16_t r = 1; r < rows; ++r) {
        for (uint16_t co = 0; co < cols; ++co) {
            c->cells[(r - 1) * cols + co] = c->cells[r * cols + co];
        }
    }
    // clear last line
    uint16_t fill = vga_entry(' ', c->color);
    for (uint16_t co = 0; co < cols; ++co) {
        c->cells[(rows - 1) * cols + co] = fill;
    }
}

static void vga_putc(Console* c, char ch) {
    int scrolled = 0;
    if (ch == '\n') {
        c->col = 0;
        if (++c->row >= c->rows) { vga_scroll_up(c); c->row = c->rows - 1; scrolled = 1; }
    } else if (ch == '\r') {
        c->col = 0;
    } else if (ch == '\b') {
        if (c->col > 0) c->col--;
    } else {
        uint16_t prow = c->row, pcol = c->col;
        c->cells[prow * c->cols + pcol] = vga_entry(ch, c->color);
        if (++c->col >= c->cols) { c->col = 0; if (++c->row >= c->rows) { vga_scroll_up(c); c->row = c->rows - 1; scrolled = 1; } }
        if (c == s_active) {
            if (scrolled) {
                vga_flush(c);
            } else {
                c->vram[prow * c->vram_pitch_chars + pcol] = c->cells[prow * c->cols + pcol];
            }
        }
    }
    if (c == s_active) {
        vga_move_hw_cursor(c->row, c->col, c->cols);
    }
}

// Public API
static void bind_backend_defaults(Console* c, volatile uint16_t* vram, uint16_t pitch_chars) {
    c->vram = vram;
    c->vram_pitch_chars = pitch_chars ? pitch_chars : c->cols;
}

void console_init(void) {
    // Initialize a default console in slot 0
    Console* c0 = &s_consoles[0];
    if (c0->cols == 0) {
        c0->cols = VGA_DEFAULT_COLS;
        c0->rows = VGA_DEFAULT_ROWS;
        c0->color = 0x0F;
        c0->row = c0->col = 0;
        bind_backend_defaults(c0, VGA_MEM_DEFAULT, VGA_DEFAULT_COLS);
        vga_clear(c0);
    } else {
        vga_flush(c0);
    }
    s_active = c0;
}

Console* console_create_vga_text(uint16_t cols, uint16_t rows) {
    // Find a free slot (skip 0 which is default if already used)
    for (size_t i = 0; i < (sizeof(s_consoles)/sizeof(s_consoles[0])); ++i) {
        Console* c = &s_consoles[i];
        // Consider an uninitialized console if cols==0
        if (c->cols == 0) {
            c->cols = (cols ? cols : VGA_DEFAULT_COLS);
            c->rows = (rows ? rows : VGA_DEFAULT_ROWS);
            c->color = 0x0F;
            c->row = c->col = 0;
            bind_backend_defaults(c, VGA_MEM_DEFAULT, c->cols);
            vga_clear(c);
            return c;
        }
    }
    return 0;
}

void console_set_active(Console* c) { if (!c) return; s_active = c; vga_flush(c); }
Console* console_get_active(void) { return s_active; }

void console_clear_ex(Console* c) { if (!c) c = s_active; if (c) vga_clear(c); }
void console_putc_ex(Console* c, char ch) { if (!c) c = s_active; if (!c) return; vga_putc(c, ch); serial_putc(ch); }
void console_write_ex(Console* c, const char* s) { if (!c) c = s_active; if (!c) return; while (*s){ char ch=*s++; vga_putc(c,ch); serial_putc(ch);} }
void console_set_color_ex(Console* c, uint8_t fg, uint8_t bg) { if (!c) c = s_active; if (!c) return; c->color = (bg<<4) | (fg & 0x0F); }

// Wrappers on active console
void console_clear(void) { console_clear_ex(0); }
void console_putc(char c) { console_putc_ex(0, c); }
void console_write(const char* s) { console_write_ex(0, s); }
void console_set_color(uint8_t fg, uint8_t bg) { console_set_color_ex(0, fg, bg); }

void console_write_hex64(uint64_t v) {
    static const char lut[16] = "0123456789ABCDEF";
    console_write("0x");
    for (int i = 15; i >= 0; --i) {
        uint8_t nyb = (v >> (i*4)) & 0xF;
        char ch = lut[nyb];
        console_putc(ch);
    }
}

void console_write_dec(uint64_t v) {
    char buf[32]; size_t i = 0;
    if (v == 0) { console_putc('0'); serial_putc('0'); return; }
    while (v > 0 && i < sizeof(buf)) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) { char ch = buf[i]; console_putc(ch); }
}

// Initialize console choosing MB2 framebuffer when present.
void console_init_from_mb2(uint64_t mb2_addr) {
    // Default first
    console_init();
    if (!mb2_addr) return;
    const mb2_tag* t = mb2_first_tag(mb2_addr);
    const mb2_tag_framebuffer* fb = NULL;
    while (t && t->type != MB2_TAG_END) {
        if (t->type == MB2_TAG_FRAMEBUFFER) { fb = (const mb2_tag_framebuffer*)t; break; }
        t = mb2_next_tag(t);
    }
    if (!fb) return;

    // Only handle EGA text type directly as drop-in replacement for VGA text.
    if (fb->common.framebuffer_type == 2 /* EGA text */ && fb->common.framebuffer_bpp == 16) {
        Console* c0 = &s_consoles[0];
        c0->cols = (uint16_t)fb->common.framebuffer_width;
        c0->rows = (uint16_t)fb->common.framebuffer_height;
        bind_backend_defaults(c0, (volatile uint16_t*)(uintptr_t)fb->common.framebuffer_addr,
                              (uint16_t)(fb->common.framebuffer_pitch / 2));
        vga_clear(c0);
        s_active = c0;
        return;
    }
    // For RGB (type=1) or indexed (type=0) we keep VGA text for now; a framebuffer
    // graphics console will be added in a follow-up.
}
