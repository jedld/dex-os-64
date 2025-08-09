#include "device.h"
#include "../console.h"
#include <stddef.h>

static void d_putc(struct device* dev, char c) { (void)dev; console_putc(c); }
static void d_write(struct device* dev, const char* s) { (void)dev; console_write(s); }
static void d_clear(struct device* dev) { (void)dev; console_clear(); }
static void d_set_color(struct device* dev, uint8_t fg, uint8_t bg) { (void)dev; console_set_color(fg,bg); }

static display_ops_t s_ops = {
    .putc = d_putc,
    .write = d_write,
    .clear = d_clear,
    .set_color = d_set_color,
};

static device_t s_disp = {
    .name = "console0",
    .type = DEV_DISPLAY,
    .ops = &s_ops,
    .priv = 0,
    .next = 0,
};

void display_console_register(void) {
    dev_register(&s_disp);
}
