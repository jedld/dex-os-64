 #include "input.h"
 #include "serial.h"
 #include "io.h"
 #include "console.h"

// --- PS/2 set 1 scancode handling with modifiers ---
static uint32_t s_mods = 0;      // MOD_* flags
static int s_e0 = 0;             // extended scancode prefix

// Base and shifted maps for non-letter keys (US layout)
static const unsigned char base_map[128] = {
    /*00*/ 0,  27,'1','2','3','4','5','6','7','8','9','0','-','=',  8,
    /*0F*/ '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    /*1F*/ 'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z',
    /*2F*/ 'x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,
};
static const unsigned char shift_map[128] = {
    /*00*/ 0,  27,'!','@','#','$','%','^','&','*','(',')','_','+',  8,
    /*0F*/ '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    /*1F*/ 'A','S','D','F','G','H','J','K','L',':','\"','~',0,'|','Z',
    /*2F*/ 'X','C','V','B','N','M','<','>','?',0,'*',0,' ',0,0,0,
};

// Recognize letter scancodes to apply CapsLock properly
static int is_letter_scancode(unsigned sc) {
    switch (sc) {
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
        case 0x1E: case 0x1F: case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26:
        case 0x2C: case 0x2D: case 0x2E: case 0x2F: case 0x30: case 0x31: case 0x32:
            return 1;
        default: return 0;
    }
}

static int ps2_status_ready(void) { return (inb(0x64) & 1) != 0; }

static int ps2_try_read_event_internal(key_event_t* ev) {
    if (!ps2_status_ready()) return 0;
    unsigned char sc = inb(0x60);
    if (sc == 0xE0) { s_e0 = 1; return 0; }
    if (sc == 0xE1) { // Pause/Break: skip a few bytes (not handled)
        return 0;
    }
    int break_code = (sc & 0x80) != 0;
    unsigned code = sc & 0x7F;

    // Modifiers
    if (!s_e0 && (code == 0x2A || code == 0x36)) { // Shift
        if (break_code) s_mods &= ~MOD_SHIFT; else s_mods |= MOD_SHIFT; s_e0 = 0; return 0;
    }
    if ((!s_e0 && code == 0x1D) || (s_e0 && code == 0x1D)) { // Ctrl (left or right)
        if (break_code) s_mods &= ~MOD_CTRL; else s_mods |= MOD_CTRL; s_e0 = 0; return 0;
    }
    if ((!s_e0 && code == 0x38) || (s_e0 && code == 0x38)) { // Alt (left or right)
        if (break_code) s_mods &= ~MOD_ALT; else s_mods |= MOD_ALT; s_e0 = 0; return 0;
    }
    if (!s_e0 && code == 0x3A && !break_code) { // Caps Lock toggle on make
        s_mods ^= MOD_CAPS; s_e0 = 0; return 0;
    }

    // Non-modifier keys
    if (break_code) { s_e0 = 0; return 0; } // only report on make

    // Extended navigation keys
    if (s_e0) {
        s_e0 = 0;
        switch (code) {
            case 0x4B: ev->type = KEY_LEFT;  ev->mods = s_mods; return 1;
            case 0x4D: ev->type = KEY_RIGHT; ev->mods = s_mods; return 1;
            case 0x48: ev->type = KEY_UP;    ev->mods = s_mods; return 1;
            case 0x50: ev->type = KEY_DOWN;  ev->mods = s_mods; return 1;
            case 0x47: ev->type = KEY_HOME;  ev->mods = s_mods; return 1;
            case 0x4F: ev->type = KEY_END;   ev->mods = s_mods; return 1;
            case 0x53: ev->type = KEY_DELETE;ev->mods = s_mods; return 1;
            case 0x49: ev->type = KEY_PGUP;  ev->mods = s_mods; return 1;
            case 0x51: ev->type = KEY_PGDN;  ev->mods = s_mods; return 1;
            default: return 0;
        }
    }

    // Regular keys
    if (code == 0x01) { ev->type = KEY_ESC; ev->mods = s_mods; return 1; }
    if (code == 0x0E) { ev->type = KEY_BACKSPACE; ev->mods = s_mods; return 1; }
    if (code == 0x0F) { ev->type = KEY_TAB; ev->mods = s_mods; return 1; }
    if (code == 0x1C) { ev->type = KEY_ENTER; ev->mods = s_mods; return 1; }

    // Printable chars
    unsigned char ch = 0;
    if (is_letter_scancode(code)) {
        // Find base letter from base_map
        ch = base_map[code]; if (!ch) return 0;
        int upper = ((s_mods & MOD_SHIFT) ? 1 : 0) ^ ((s_mods & MOD_CAPS) ? 1 : 0);
        if (upper && ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 'a' + 'A');
        if (!upper && ch >= 'A' && ch <= 'Z') ch = (unsigned char)(ch - 'A' + 'a');
    } else {
        if (s_mods & MOD_SHIFT) ch = shift_map[code]; else ch = base_map[code];
    }
    if (ch) { ev->type = KEY_CHAR; ev->ch = ch; ev->mods = s_mods; return 1; }
    return 0;
}

int input_try_read_event(key_event_t* ev) {
    // First PS/2 keyboard
    if (ps2_try_read_event_internal(ev)) return 1;
    // Then serial
    int c = serial_try_getc();
    if (c >= 0) {
        ev->mods = 0; ev->ch = 0;
        if (c == '\r' || c == '\n') { ev->type = KEY_ENTER; return 1; }
        if (c == 8 || c == 127) { ev->type = KEY_BACKSPACE; return 1; }
        if (c == 27) { ev->type = KEY_ESC; return 1; }
        if (c == '\t') { ev->type = KEY_TAB; return 1; }
        if (c >= 32 && c < 127) { ev->type = KEY_CHAR; ev->ch = (uint16_t)c; return 1; }
    }
    return 0;
}

void input_read_event(key_event_t* ev) {
    while (!input_try_read_event(ev)) { /* spin */ }
}

int input_try_getc(void) {
    key_event_t ev;
    if (!input_try_read_event(&ev)) return -1;
    switch (ev.type) {
        case KEY_CHAR: return (int)(ev.ch & 0xFF);
        case KEY_ENTER: return '\n';
        case KEY_BACKSPACE: return '\b';
        default: return -1;
    }
}

int input_getc(void) {
    int c;
    while ((c = input_try_getc()) < 0) { /* spin */ }
    return c;
}

// Reprint tail from index i to len, then move cursor back by (len - i)
static void reprint_from(const char* buf, uint64_t i, uint64_t len) {
    for (uint64_t k = i; k < len; ++k) console_putc(buf[k]);
    for (uint64_t k = i; k < len; ++k) console_putc('\b');
}

uint64_t input_readline(char* buf, uint64_t max_len) {
    if (max_len == 0) return 0;
    uint64_t len = 0;
    uint64_t cur = 0; // cursor position within [0..len]
    for (;;) {
        key_event_t ev; input_read_event(&ev);
        if (ev.type == KEY_ENTER) { console_putc('\n'); break; }
    if (ev.type == KEY_PGUP)  { console_page_up();  continue; }
    if (ev.type == KEY_PGDN)  { console_page_down(); continue; }
        if (ev.type == KEY_LEFT) {
            if (cur > 0) { console_putc('\b'); cur--; }
            continue;
        }
        if (ev.type == KEY_RIGHT) {
            if (cur < len) { console_putc(buf[cur]); cur++; }
            continue;
        }
        if (ev.type == KEY_HOME) {
            // Ctrl+Home: jump to oldest scrollback (viewport)
            if (ev.mods & MOD_CTRL) { console_page_home(); continue; }
            while (cur > 0) { console_putc('\b'); cur--; }
            continue;
        }
        if (ev.type == KEY_END) {
            // Ctrl+End: jump to live output (viewport end)
            if (ev.mods & MOD_CTRL) { console_page_end(); continue; }
            while (cur < len) { console_putc(buf[cur]); cur++; }
            continue;
        }
        if (ev.type == KEY_BACKSPACE) {
            if (cur > 0) {
                // Move cursor left one
                console_putc('\b');
                // Remove character before cursor (now at position cur-1)
                for (uint64_t i = cur - 1; i + 1 < len; ++i) buf[i] = buf[i+1];
                cur--; len--;
                // Reprint tail starting at new cur
                for (uint64_t k = cur; k < len; ++k) console_putc(buf[k]);
                // Clear last leftover character on screen
                console_putc(' ');
                // Move cursor back to cur position
                for (uint64_t k = cur; k <= len; ++k) console_putc('\b');
            }
            continue;
        }
        if (ev.type == KEY_DELETE) {
            if (cur < len) {
                // Remove character at cursor
                for (uint64_t i = cur; i + 1 < len; ++i) buf[i] = buf[i+1];
                len--;
                // Reprint tail starting at cur
                for (uint64_t k = cur; k < len; ++k) console_putc(buf[k]);
                // Clear last leftover character on screen
                console_putc(' ');
                // Move cursor back to original cur position
                for (uint64_t k = cur; k <= len; ++k) console_putc('\b');
            }
            continue;
        }
        if (ev.type == KEY_CHAR) {
            if (len + 1 < max_len) {
                // insert at cur
                for (uint64_t i = len; i > cur; --i) buf[i] = buf[i-1];
                buf[cur] = (char)ev.ch; len++; console_putc((char)ev.ch); cur++;
                // reprint rest
                reprint_from(buf, cur, len);
            }
            continue;
        }
    }
    return len;
}
