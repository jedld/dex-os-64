#include "block.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t* data;
    uint64_t bytes;
} ramdisk_t;

static int rd_read(block_device_t* dev, uint64_t lba, void* buf, uint32_t count) {
    ramdisk_t* rd = (ramdisk_t*)dev->priv;
    uint64_t off = lba * dev->sector_size;
    uint64_t len = (uint64_t)count * dev->sector_size;
    if (off + len > rd->bytes) return -1;
    uint8_t* dst = (uint8_t*)buf; const uint8_t* src = rd->data + off;
    for (uint64_t i = 0; i < len; ++i) dst[i] = src[i];
    return (int)count;
}
static int rd_write(block_device_t* dev, uint64_t lba, const void* buf, uint32_t count) {
    ramdisk_t* rd = (ramdisk_t*)dev->priv;
    uint64_t off = lba * dev->sector_size;
    uint64_t len = (uint64_t)count * dev->sector_size;
    if (off + len > rd->bytes) return -1;
    const uint8_t* src = (const uint8_t*)buf; uint8_t* dst = rd->data + off;
    for (uint64_t i = 0; i < len; ++i) dst[i] = src[i];
    return (int)count;
}

static const block_ops_t s_ops = { rd_read, rd_write };

// Factory: create and register a RAM disk backed by the kernel heap buffer.
extern void* kmalloc(size_t);
extern void console_write(const char*);
extern void console_write_hex64(uint64_t);

int ramdisk_create(const char* name, uint64_t bytes) {
    if (!name || bytes == 0) return -1;
    // Round bytes to sector size
    uint32_t sec = block_default_sector();
    uint64_t rounded = (bytes + sec - 1) / sec * sec;
    ramdisk_t* rd = (ramdisk_t*)kmalloc(sizeof(ramdisk_t));
    if (!rd) return -1;
    rd->data = (uint8_t*)kmalloc((size_t)rounded);
    if (!rd->data) return -1;
    rd->bytes = rounded;
    block_device_t* bd = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!bd) return -1;
    // Fill device
    // Simple strncpy
    int i=0; for (; i<15 && name[i]; ++i) bd->name[i]=name[i]; bd->name[i]=0;
    bd->sector_size = sec;
    bd->sector_count = rounded / sec;
    bd->ops = &s_ops;
    bd->priv = rd;
    bd->next = NULL;
    block_register(bd);
    console_write("ramdisk created: "); console_write(bd->name); console_write(" bytes=0x"); console_write_hex64(rounded); console_write("\n");
    return 0;
}
