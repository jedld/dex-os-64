#pragma once
#include <stdint.h>

typedef struct block_device block_device_t;

typedef struct {
    int (*read)(block_device_t* dev, uint64_t lba, void* buf, uint32_t count);  // count in sectors
    int (*write)(block_device_t* dev, uint64_t lba, const void* buf, uint32_t count); // optional, -1 if RO
} block_ops_t;

struct block_device {
    char name[16];
    uint32_t sector_size;   // bytes per sector
    uint64_t sector_count;  // total sectors
    const block_ops_t* ops;
    void* priv;
    block_device_t* next;
};

void block_register(block_device_t* dev);
block_device_t* block_find(const char* name);
uint32_t block_default_sector(void);
