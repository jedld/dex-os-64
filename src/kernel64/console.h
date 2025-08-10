#pragma once
#include <stdint.h>

// Opaque console type supporting multiple instances (VGA text backend for now)
typedef struct Console Console;

// Initialize console subsystem and create a default active console.
// Prefer Multiboot2 framebuffer if provided (EGA text or RGB handled progressively).
void console_init(void);
void console_init_from_mb2(uint64_t mb2_addr);

// Create an additional VGA text console instance (cols x rows). Returns NULL on failure.
Console* console_create_vga_text(uint16_t cols, uint16_t rows);

// Switch active console used by the wrapper APIs below and bind to VGA output.
void console_set_active(Console* c);
// Get the current active console.
Console* console_get_active(void);

// Per-console operations
void console_clear_ex(Console* c);
void console_putc_ex(Console* c, char ch);
void console_write_ex(Console* c, const char* s);
void console_set_color_ex(Console* c, uint8_t fg, uint8_t bg);

// Convenience wrappers that operate on the active console
void console_clear(void);
void console_putc(char c);
void console_write(const char* s);
void console_write_hex64(uint64_t v);
void console_write_dec(uint64_t v);
void console_set_color(uint8_t fg, uint8_t bg);

// Scrollback/page view controls for the active console
// Maintain a bounded scrollback buffer and allow PageUp/PageDown style navigation.
// By default, one "page" equals (rows-1) lines.
void console_page_up(void);
void console_page_down(void);
void console_page_home(void); // jump to oldest available
void console_page_end(void);  // jump back to live output

// Console geometry hint (defaults for initial console)
#ifndef CONSOLE_COLS
#define CONSOLE_COLS 80
#endif

// Tiny strlen to avoid pulling in hosted headers
static inline uint64_t console_strlen(const char* s) {
	uint64_t n = 0; if (!s) return 0; while (s[n]) ++n; return n;
}

// Render a progress bar on one line and keep cursor on that line.
// Strategy: CR, clear the line with spaces, CR again, then print trimmed content.
static inline void console_progress(const char* label, uint64_t done, uint64_t total) {
	if (total == 0) total = 1;
	uint64_t pct = (done * 100) / total; if (pct > 100) pct = 100;
	uint64_t pct_len = (pct >= 100) ? 3 : (pct >= 10 ? 2 : 1);

	// Clear current line (avoid wrapping: write at most COLS-1 spaces)
	console_putc('\r');
	for (uint64_t i = 0; i < (CONSOLE_COLS - 1); ++i) console_putc(' ');
	console_putc('\r');

	// Trim label to fit
	uint64_t lbl_len = console_strlen(label);
	// Reserve space: " [" + "] " + pct + "%" = 2 + 2 + pct_len + 1 = 5 + pct_len
	// We also keep at least 10 chars for the bar.
	uint64_t min_bar = 10, max_bar = 50;
	uint64_t reserved = 1 /*space*/ + 1 /*[*/ + 1 /*]*/ + 1 /*space*/ + pct_len + 1 /*%*/;
	// Ensure room for bar and reserved; keep total < CONSOLE_COLS-1 to avoid wrap.
	uint64_t max_lbl = (CONSOLE_COLS > (reserved + min_bar + 1)) ? (CONSOLE_COLS - 1 - reserved - min_bar) : 0;
	if (lbl_len > max_lbl) lbl_len = max_lbl;

	// Determine bar width
	uint64_t bar_width = CONSOLE_COLS - 1 - reserved - lbl_len;
	if (bar_width > max_bar) bar_width = max_bar;
	if (bar_width < min_bar) bar_width = min_bar;

	// Print label (trimmed)
	for (uint64_t i = 0; i < lbl_len; ++i) console_putc(label[i]);
	console_putc(' ');
	console_putc('[');
	// Fill bar
	uint64_t bars = (pct * bar_width) / 100;
	for (uint64_t i = 0; i < bar_width; ++i) console_putc(i < bars ? '#' : '.');
	console_putc(']');
	console_putc(' ');
	console_write_dec(pct);
	console_putc('%');
}
