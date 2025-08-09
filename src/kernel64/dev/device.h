#pragma once
#include <stdint.h>

typedef enum {
    DEV_DISPLAY = 1,
    DEV_KEYBOARD = 2,
} dev_type_t;

struct device;

typedef struct {
    void (*putc)(struct device*, char c);
    void (*write)(struct device*, const char* s);
    void (*clear)(struct device*);
    void (*set_color)(struct device*, uint8_t fg, uint8_t bg);
} display_ops_t;

typedef struct {
    int  (*try_getc)(struct device*);
    int  (*getc)(struct device*);
} keyboard_ops_t;

typedef struct device {
    const char* name;
    dev_type_t type;
    void* ops;      // display_ops_t* or keyboard_ops_t*
    void* priv;
    struct device* next;
} device_t;

void dev_register(device_t* dev);
device_t* dev_first_of_type(dev_type_t type);
device_t* dev_find_by_name(const char* name);
