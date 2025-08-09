 #include "input.h"
 #include "serial.h"
 #include "io.h"
 #include "console.h"

static const unsigned char scancode_to_ascii[128] = {
    /*00*/ 0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    /*0F*/ '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    /*1F*/ 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,  '\\', 'z',
    /*2F*/ 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,    0,   0,
    /*3F*/ 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,
    /*4F*/ 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,
    /*5F*/ 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,
    /*6F*/ 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    0,   0,
};

static int ps2_try_get_ascii(void) {
    if ((inb(0x64) & 1) == 0) return -1;
    unsigned char sc = inb(0x60);
    if (sc == 0xE0) return -1;            // ignore extended
    if (sc & 0x80) return -1;             // key release
    if (sc < sizeof(scancode_to_ascii)) {
        unsigned char ch = scancode_to_ascii[sc];
        if (ch == 0) return -1;
        return (int)ch;
    }
    return -1;
}

int input_try_getc(void) {
    int ch = ps2_try_get_ascii();
    if (ch >= 0) return ch;
    ch = serial_try_getc();
    if (ch >= 0) return ch;
    return -1;
}

int input_getc(void) {
    int c;
    while ((c = input_try_getc()) < 0) { /* spin */ }
    return c;
}

uint64_t input_readline(char* buf, uint64_t max_len) {
    uint64_t n = 0;
    if (max_len == 0) return 0;
    for (;;) {
        int c = input_getc();
        if (c == '\r') c = '\n';
        if (c == '\b' || c == 127) {
            if (n > 0) {
                n--; console_putc('\b'); console_putc(' '); console_putc('\b');
            }
            continue;
        }
        if (c == '\n') {
            console_putc('\n');
            break;
        }
    if (n + 1 < max_len) {
            buf[n++] = (char)c;
            console_putc((char)c);
        }
    }
    return n;
}
