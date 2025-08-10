#include "block.h"
#include <stddef.h>

static block_device_t* g_head = NULL;
extern void serial_putc(char);
static void slog(const char* s){ while(*s) serial_putc(*s++); serial_putc('\n'); }

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

typedef struct {
    block_device_t* parent;
    uint64_t lba_base;
    uint64_t lba_count;
} part_priv_t;

static int part_read(block_device_t* dev, uint64_t lba, void* buf, uint32_t count){
    part_priv_t* p = (part_priv_t*)dev->priv;
    if (!p || !p->parent || !p->parent->ops || !p->parent->ops->read) return -1;
    if (lba + count > p->lba_count) return -1;
    return p->parent->ops->read(p->parent, p->lba_base + lba, buf, count);
}
static int part_write(block_device_t* dev, uint64_t lba, const void* buf, uint32_t count){
    part_priv_t* p = (part_priv_t*)dev->priv;
    if (!p || !p->parent || !p->parent->ops || !p->parent->ops->write) return -1;
    if (lba + count > p->lba_count) return -1;
    return p->parent->ops->write(p->parent, p->lba_base + lba, buf, count);
}

static block_ops_t part_ops;

extern void* kmalloc(size_t);
extern void console_write(const char*);
extern void console_write_hex64(uint64_t);

void block_scan_partitions(void){
    uint8_t mbr[512];
    slog("[block] scan_partitions enter");
    for (block_device_t* d = g_head; d; d = d->next) {
    console_write("[block] probe dev "); console_write(d->name);
    console_write(" sec="); console_write_hex64((uint64_t)d->sector_size);
    console_write(" count="); console_write_hex64((uint64_t)d->sector_count);
    console_write("\n");
    // Proceed to scan all devices (including RAM disks)
    if (!d->ops || !d->ops->read || d->sector_size != 512) continue;
#ifdef BLOCK_DEBUG
    // Log the function pointer addresses to ensure we're calling what we expect
    console_write("[block] ops@="); console_write_hex64((uint64_t)(uintptr_t)d->ops);
    console_write(" read@="); console_write_hex64((uint64_t)(uintptr_t)d->ops->read);
    console_write(" write@="); console_write_hex64((uint64_t)(uintptr_t)d->ops->write);
    console_write(" dev@="); console_write_hex64((uint64_t)(uintptr_t)d);
    console_write(" priv@="); console_write_hex64((uint64_t)(uintptr_t)d->priv);
    console_write("\n");
    // Dump first few qwords of ops struct to catch corruption/misalignment
    const uint64_t* op64 = (const uint64_t*)(const void*)d->ops;
    console_write("[block] ops[0..3] = ");
    for (int oi=0; oi<4; ++oi) { console_write_hex64(op64[oi]); console_write(" "); }
    console_write("\n");
#endif
    console_write("[block] reading MBR\n");
#ifdef BLOCK_DEBUG
    // Test-call the read op with count=0 and then 1
    (void)d->ops->read(d, 0, mbr, 0);
    uint8_t scratch[512];
    (void)d->ops->read(d, 0, scratch, 1);
#endif
    // Now perform the actual read of LBA0
    if (d->ops->read(d, 0, mbr, 1) != 1) { console_write("[block] read LBA0 fail\n"); continue; }
        console_write("[block] LBA0 ok\n");
        if (mbr[510] != 0x55 || mbr[511] != 0xAA) { console_write("[block] no 0x55AA\n"); continue; }
        console_write("[block] 0x55AA found\n");
        for (int i=0;i<4;++i){
            const uint8_t* p = &mbr[446 + i*16];
            uint8_t type = p[4];
            uint32_t start = *(const uint32_t*)&p[8];
            uint32_t count = *(const uint32_t*)&p[12];
            if (type == 0 || count == 0) continue;
            console_write("[block] part entry found idx="); console_write_hex64(i); console_write(" type="); console_write_hex64(type); console_write(" start="); console_write_hex64(start); console_write(" count="); console_write_hex64(count); console_write("\n");
            block_device_t* pd = (block_device_t*)kmalloc(sizeof(block_device_t)); if(!pd) continue;
            part_priv_t* pp = (part_priv_t*)kmalloc(sizeof(part_priv_t)); if(!pp){ /* leak pd */ continue; }
            pp->parent = d; pp->lba_base = start; pp->lba_count = count;
            // Name like <parent>pN
            int nlen=0; while (d->name[nlen] && nlen<15) nlen++;
            int idx = 0; char name[16];
            for (; idx<nlen && idx<12; ++idx) name[idx] = d->name[idx];
            if (idx<13) { name[idx++]='p'; name[idx++] = '1' + i; }
            name[idx]=0;
            int j=0; for(; name[j]; ++j) pd->name[j]=name[j]; pd->name[j]=0;
            pd->sector_size = d->sector_size;
            pd->sector_count = pp->lba_count;
            // Initialize part ops on first use
            part_ops.read = part_read; part_ops.write = part_write;
            pd->ops = &part_ops;
            pd->priv = pp; pd->next = NULL;
            block_register(pd);
            console_write("partition: "); console_write(pd->name); console_write(" base="); console_write_hex64(pp->lba_base); console_write(" count="); console_write_hex64(pp->lba_count); console_write("\n");
        }
    }
    slog("[block] scan_partitions exit");
}
