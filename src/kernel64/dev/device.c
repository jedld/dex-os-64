#include "device.h"
#include <stddef.h>

static device_t* s_dev_list = NULL;

void dev_register(device_t* dev) {
    if (!dev) return;
    dev->next = s_dev_list;
    s_dev_list = dev;
}

device_t* dev_first_of_type(dev_type_t type) {
    for (device_t* d = s_dev_list; d; d = d->next) {
        if (d->type == type) return d;
    }
    return NULL;
}

device_t* dev_find_by_name(const char* name) {
    if (!name) return NULL;
    for (device_t* d = s_dev_list; d; d = d->next) {
        const char* a = d->name; const char* b = name;
        while (*a && *b && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0) return d;
    }
    return NULL;
}
