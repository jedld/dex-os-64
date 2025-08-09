#pragma once
#include <stdint.h>

// Modifier flags
#define MOD_SHIFT  (1u<<0)
#define MOD_CTRL   (1u<<1)
#define MOD_ALT    (1u<<2)
#define MOD_CAPS   (1u<<3)

typedef enum {
	KEY_NONE = 0,
	KEY_CHAR,       // printable char in 'ch'
	KEY_ENTER,
	KEY_BACKSPACE,
	KEY_TAB,
	KEY_ESC,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_HOME,
	KEY_END,
	KEY_DELETE,
} key_type_t;

typedef struct {
	key_type_t type;
	uint16_t ch;     // valid when type==KEY_CHAR
	uint32_t mods;   // MOD_* bitmask
} key_event_t;

// Non-blocking: returns 1 if an event was read into *ev, 0 if none.
int input_try_read_event(key_event_t* ev);
// Blocking: waits until an event is available and fills *ev.
void input_read_event(key_event_t* ev);

// Backward-compat: ASCII-oriented input
// Non-blocking getc: returns byte [0..255] or -1 if no printable char
int input_try_getc(void);
// Blocking getc: waits for a printable char or newline/backspace and returns it
int input_getc(void);

// Read a line with basic editing (arrows/home/end/delete/backspace). Returns length (no '\0' stored)
uint64_t input_readline(char* buf, uint64_t max_len);
