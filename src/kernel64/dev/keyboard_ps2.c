#include "device.h"
#include "../io.h"
#include "../serial.h"
#include <stddef.h>

static int ps2_try_getc_dev(struct device* dev) {
    (void)dev;
    // Check PS/2 controller status
    if ((inb(0x64) & 1) == 0) return -1;
    unsigned char sc = inb(0x60);
    // Very simple: accept ASCII-only subset via map from input.c
    if (sc == 0xE0) return -1;
    if (sc & 0x80) return -1;
    // Minimal US scancode set 1 mapping
    static const unsigned char map[128] = {
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=',8,
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z',
        'x','c','v','b','n','m',',','.','/',0,'*',0,' ',0,0,0,
    };
    if (sc < sizeof(map)) {
        unsigned char ch = map[sc];
        if (ch) return (int)ch;
    }
    return -1;
}

static int kb_getc(struct device* dev) {
    int c;
    while ((c = ps2_try_getc_dev(dev)) < 0) {
        int s = serial_try_getc();
        if (s >= 0) return s;
    }
    return c;
}

static keyboard_ops_t s_ops = {
    .try_getc = ps2_try_getc_dev,
    .getc = kb_getc,
};

static device_t s_kb = {
    .name = "ps2kbd0",
    .type = DEV_KEYBOARD,
    .ops = &s_ops,
    .priv = 0,
    .next = 0,
};

void kb_ps2_register(void) {
    dev_register(&s_kb);
}
