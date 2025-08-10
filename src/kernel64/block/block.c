#include "block.h"
#include <stddef.h>

static block_device_t* g_head = NULL;

void block_register(block_device_t* dev) {
    if (!dev) return;
    dev->next = g_head;
    g_head = dev;
}

block_device_t* block_find(const char* name) {
    for (block_device_t* d = g_head; d; d = d->next) {
        const char* a = name; const char* b = d->name;
        while (*a && *b && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0) return d;
    }
    return NULL;
}

uint32_t block_default_sector(void) { return 512; }

block_device_t* block_first(void){ return g_head; }
block_device_t* block_next(block_device_t* dev){ return dev?dev->next:NULL; }
