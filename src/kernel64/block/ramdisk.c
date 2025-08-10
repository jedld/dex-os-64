#include "block.h"
#include "../serial.h"
#include <stdint.h>
#include <stddef.h>
#include "../../kernel/mm/pmm.h"

typedef struct {
    uint8_t* data;
    uint64_t bytes;
} ramdisk_t;

static int rd_read(block_device_t* dev, uint64_t lba, void* buf, uint32_t count) {
    ramdisk_t* rd = (ramdisk_t*)dev->priv;
    // Minimal serial breadcrumb to avoid console reentrancy during early boot
    serial_putc('E');
    // Log pointers to help diagnose call target and buffer addresses
    extern void console_write(const char*);
    extern void console_write_hex64(uint64_t);
    console_write("[ramdisk] rd_read enter dev="); console_write_hex64((uint64_t)(uintptr_t)dev);
    console_write(" priv="); console_write_hex64((uint64_t)(uintptr_t)dev->priv);
    console_write(" buf="); console_write_hex64((uint64_t)(uintptr_t)buf);
    console_write(" lba="); console_write_hex64(lba);
    console_write(" cnt="); console_write_hex64(count);
    console_write("\n");
    if (count == 0) return 0;
    uint64_t off = lba * dev->sector_size;
    uint64_t len = (uint64_t)count * dev->sector_size;
    if (off + len > rd->bytes) return -1;
    uint8_t* dst = (uint8_t*)buf; const uint8_t* src = rd->data + off;
    // Quick probe: touch first and last byte of destination to detect faults early
    volatile uint8_t tmp_probe = 0;
    tmp_probe ^= dst[0];
    if (len > 0) { tmp_probe ^= dst[len - 1]; }
    for (uint64_t i = 0; i < len; ++i) dst[i] = src[i];
    serial_putc('X');
    console_write("[ramdisk] rd_read exit len="); console_write_hex64(len);
    console_write(" dst="); console_write_hex64((uint64_t)(uintptr_t)dst);
    console_write(" src="); console_write_hex64((uint64_t)(uintptr_t)src);
    console_write("\n");
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

// Avoid static const init of function pointers (no relocations at runtime).
static block_ops_t s_ops;

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
    // Allocate backing store from physical frames to avoid exhausting the early heap
    uint64_t pages = (rounded + 4095ULL) / 4096ULL;
    // Ensure allocation stays below 1GiB to match conservative identity map
    extern uint64_t pmm_alloc_frames_below(size_t count, uint64_t max_phys_exclusive);
    uint64_t paddr = pmm_alloc_frames_below((size_t)pages, 1ULL<<30);
    if (!paddr) { return -1; }
    rd->data = (uint8_t*)(uintptr_t)paddr; // identity-mapped
    console_write("ramdisk phys base=0x"); console_write_hex64((uint64_t)paddr); console_write(" size=0x"); console_write_hex64((uint64_t)rounded); console_write("\n");
    // Zero the device
    for (uint64_t i = 0; i < rounded; ++i) rd->data[i] = 0;
    rd->bytes = rounded;
    block_device_t* bd = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!bd) return -1;
    // Fill device
    // Simple strncpy
    int i=0; for (; i<15 && name[i]; ++i) bd->name[i]=name[i]; bd->name[i]=0;
    bd->sector_size = sec;
    bd->sector_count = rounded / sec;
    // Initialize ops with runtime addresses
    s_ops.read = rd_read;
    s_ops.write = rd_write;
    bd->ops = &s_ops;
    bd->priv = rd;
    bd->next = NULL;
    // Log the ops->read/write addresses for cross-checking with block layer
    console_write("[ramdisk] s_ops="); console_write_hex64((uint64_t)(uintptr_t)&s_ops);
    console_write(" read@="); console_write_hex64((uint64_t)(uintptr_t)s_ops.read);
    console_write(" write@="); console_write_hex64((uint64_t)(uintptr_t)s_ops.write);
    console_write(" &rd_read="); console_write_hex64((uint64_t)(uintptr_t)&rd_read);
    console_write(" &rd_write="); console_write_hex64((uint64_t)(uintptr_t)&rd_write);
    console_write(" bd@="); console_write_hex64((uint64_t)(uintptr_t)bd);
    console_write(" priv@="); console_write_hex64((uint64_t)(uintptr_t)bd->priv);
    console_write("\n");
    block_register(bd);
    console_write("ramdisk created: "); console_write(bd->name); console_write(" bytes=0x"); console_write_hex64(rounded); console_write("\n");
    return 0;
}
