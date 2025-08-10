#include "block.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t* base;      // identity-mapped virtual address to memory region
    uint64_t bytes;     // total bytes
    int writable;       // 0 = read-only, 1 = writable
} memdisk_priv_t;

static int mem_read(block_device_t* dev, uint64_t lba, void* buf, uint32_t count){
    memdisk_priv_t* p = (memdisk_priv_t*)dev->priv;
    if (!p || !buf) return -1;
    uint64_t off = lba * dev->sector_size;
    uint64_t need = (uint64_t)count * dev->sector_size;
    if (off + need > p->bytes) return -1;
    const uint8_t* src = p->base + off; uint8_t* dst = (uint8_t*)buf;
    for (uint64_t i=0;i<need;++i) dst[i] = src[i];
    return (int)count;
}
static int mem_write(block_device_t* dev, uint64_t lba, const void* buf, uint32_t count){
    memdisk_priv_t* p = (memdisk_priv_t*)dev->priv;
    if (!p || !buf || !p->writable) return -1;
    uint64_t off = lba * dev->sector_size;
    uint64_t need = (uint64_t)count * dev->sector_size;
    if (off + need > p->bytes) return -1;
    const uint8_t* src = (const uint8_t*)buf; uint8_t* dst = p->base + off;
    for (uint64_t i=0;i<need;++i) dst[i] = src[i];
    return (int)count;
}

static block_ops_t mem_ops;

extern void* kmalloc(size_t);

// Create a memory-backed block device named `name` for [base, base+bytes)
// sector_size must divide bytes; writable=0 for read-only.
int memdisk_register(const char* name, void* base, uint64_t bytes, uint32_t sector_size, int writable){
    if (!name || !base || bytes==0 || sector_size==0) return -1;
    if (bytes % sector_size) return -1;
    block_device_t* d = (block_device_t*)kmalloc(sizeof(block_device_t)); if(!d) return -1;
    memdisk_priv_t* p = (memdisk_priv_t*)kmalloc(sizeof(memdisk_priv_t)); if(!p) return -1;
    p->base = (uint8_t*)base; p->bytes = bytes; p->writable = writable;
    int i=0; for(; name[i] && i<15; ++i) d->name[i]=name[i]; d->name[i]=0;
    d->sector_size = sector_size;
    d->sector_count = bytes / sector_size;
    mem_ops.read = mem_read; mem_ops.write = mem_write;
    d->ops = &mem_ops; d->priv = p; d->next = NULL;
    block_register(d);
    return 0;
}
