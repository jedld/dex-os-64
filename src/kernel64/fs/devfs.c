#include "../vfs/vfs.h"
#include "../console.h"
#include "../block/block.h"
#include <stdint.h>
#include <stddef.h>
#include "../../kernel/mm/kmalloc.h"
#include "../serial.h"

// Simple devfs exposing block devices under /dev
// Path format: "/" lists entries; names are device names (e.g., ram0)

typedef struct {
    int is_dir;
    char name[16];
    uint64_t size;
    block_device_t* bdev;
} devfs_node_t;

// Build ops table at runtime to avoid absolute function-pointer addresses
// in rodata (image is position-independent at runtime).
static vfs_fs_ops_t devfs_ops;

static int devfs_mount(block_device_t* bdev, const char* mname, void** out_priv) {
    (void)bdev; (void)mname;
    { const char* s="[devfs] mount enter\n"; while(*s) serial_putc(*s++); }
    if (!out_priv) return -1;
    *out_priv = (void*)1;
    { const char* s="[devfs] mount exit\n"; while(*s) serial_putc(*s++); }
    return 0;
}

static void devfs_umount(void* p){ (void)p; }

static int path_is_root(const char* sub){
    return (!sub) || (*sub=='\0') || ((*sub=='/'||*sub=='\0') && sub[1]=='\0');
}

static vfs_node_t* devfs_open(void* fs_priv, const char* path){
    (void)fs_priv;
    devfs_node_t* dn=(devfs_node_t*)kmalloc(sizeof(devfs_node_t));
    if(!dn) return NULL;
    vfs_node_t* vn=(vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if(!vn){ kfree(dn); return NULL; }
    vn->fops=&devfs_ops; vn->fs_priv=fs_priv; vn->file_priv=dn;

    if (!path || path[0]=='\0' || (path[0]=='/' && path[1]=='\0')){
        dn->is_dir=1; dn->bdev=NULL; dn->name[0]=0; dn->size=0; return vn;
    }
    const char* q=path; if(*q=='/') ++q; if(!*q){ dn->is_dir=1; dn->bdev=NULL; dn->name[0]=0; dn->size=0; return vn; }
    // lookup block device by name
    block_device_t* b = block_find(q);
    if(!b){ kfree(dn); kfree(vn); return NULL; }
    dn->is_dir=0; dn->bdev=b; // size in bytes for stat
    uint64_t bytes = b->sector_count * (uint64_t)b->sector_size;
    dn->size = bytes;
    // copy name
    int i=0; for(; i<15 && b->name[i]; ++i) dn->name[i]=b->name[i]; dn->name[i]=0;
    return vn;
}

static int devfs_stat(void* fs_priv, const char* path, uint64_t* size, int* is_dir){
    (void)fs_priv;
    if (!path || path_is_root(path)){ if(size)*size=0; if(is_dir)*is_dir=1; return 0; }
    const char* q=path; if(*q=='/') ++q; if(!*q){ if(size)*size=0; if(is_dir)*is_dir=1; return 0; }
    block_device_t* b=block_find(q); if(!b) return -1;
    if(size) *size = b->sector_count * (uint64_t)b->sector_size;
    if(is_dir)*is_dir=0; return 0;
}

static int devfs_readdir(vfs_node_t* n, uint32_t idx, char* name_out, uint32_t maxlen){
    if(!n||!name_out||maxlen==0) return -1;
    devfs_node_t* dn=(devfs_node_t*)n->file_priv;
    if(!dn) { name_out[0]=0; return 0; }
    if(!dn->is_dir){ name_out[0]=0; return 0; }
    
    uint32_t i=0;
    for(block_device_t* b=block_first(); b; b=block_next(b)){
        if(i==idx){
            uint32_t j=0; while(b->name[j] && j<maxlen-1){ name_out[j]=b->name[j]; ++j; }
            name_out[j]=0; return 1;
        }
        i++;
    }
    name_out[0]=0; return 0;
}

// Read from an underlying block device with support for unaligned head/tail via read-modify-copy
static int devfs_read(vfs_node_t* n, uint64_t off, void* buf, uint64_t len){
    if(!n||!buf) return -1;
    devfs_node_t* dn=(devfs_node_t*)n->file_priv; if(dn->is_dir) return -1;
    block_device_t* b=dn->bdev; if(!b||!b->ops||!b->ops->read) return -1;
    uint32_t sec=b->sector_size; uint64_t end=off+len; if(end > dn->size) end = dn->size; if(end<=off) return 0; len = end - off;
    uint64_t first_lba = off / sec; uint32_t head_off = (uint32_t)(off % sec); uint64_t remaining = len; uint8_t* out=(uint8_t*)buf; uint64_t pos=0;
    // handle head partial
    if(head_off!=0){
        uint8_t* tmp=(uint8_t*)kmalloc(sec); if(!tmp) return -1;
        if (b->ops->read(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        uint32_t take = sec - head_off; if (take > remaining) take = (uint32_t)remaining;
        for(uint32_t i=0;i<take; ++i) out[pos+i] = tmp[head_off+i];
        pos+=take; remaining-=take; first_lba++; kfree(tmp);
    }
    // handle middle full sectors
    while(remaining >= sec){
        uint32_t cnt = (remaining / sec); if (cnt > 128) cnt = 128;
        if (b->ops->read(b, first_lba, out+pos, cnt) != (int)cnt) return -1;
        uint64_t adv = (uint64_t)cnt * sec; pos += adv; remaining -= adv; first_lba += cnt;
    }
    // handle tail partial
    if(remaining>0){
        uint8_t* tmp=(uint8_t*)kmalloc(sec); if(!tmp) return -1;
        if(b->ops->read(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        for(uint32_t i=0;i<remaining;++i) out[pos+i] = tmp[i];
        pos += remaining; remaining = 0; kfree(tmp);
    }
    return (int)pos;
}

static int devfs_write(vfs_node_t* n, uint64_t off, const void* buf, uint64_t len){
    if(!n||!buf) return -1;
    devfs_node_t* dn=(devfs_node_t*)n->file_priv; if(dn->is_dir) return -1;
    block_device_t* b=dn->bdev; if(!b->ops->write) return -1;
    uint32_t sec=b->sector_size; uint64_t end=off+len; if(end > dn->size) end = dn->size; if(end<=off) return 0; len = end - off;
    const uint8_t* in=(const uint8_t*)buf; uint64_t first_lba = off / sec; uint32_t head_off = (uint32_t)(off % sec); uint64_t remaining=len; uint64_t pos=0;
    if(head_off!=0){
        uint8_t* tmp=(uint8_t*)kmalloc(sec); if(!tmp) return -1;
        if(b->ops->read(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        uint32_t put = sec - head_off; if(put>remaining) put=(uint32_t)remaining;
        for(uint32_t i=0;i<put;++i) tmp[head_off+i] = in[pos+i];
        if(b->ops->write(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        pos+=put; remaining-=put; first_lba++; kfree(tmp);
    }
    while(remaining >= sec){
        uint32_t cnt=(remaining/sec); if(cnt>128) cnt=128;
        if(b->ops->write(b, first_lba, in+pos, cnt) != (int)cnt) return -1;
        uint64_t adv=(uint64_t)cnt*sec; pos+=adv; remaining-=adv; first_lba+=cnt;
    }
    if(remaining>0){
        uint8_t* tmp=(uint8_t*)kmalloc(sec); if(!tmp) return -1;
        if(b->ops->read(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        for(uint32_t i=0;i<remaining;++i) tmp[i]=in[pos+i];
        if(b->ops->write(b, first_lba, tmp, 1) != 1) { kfree(tmp); return -1; }
        pos+=remaining; remaining=0; kfree(tmp);
    }
    return (int)pos;
}

void devfs_register(void){
    // Fill ops at runtime so function addresses are computed with RIP-relative code
    devfs_ops.mount   = devfs_mount;
    devfs_ops.umount  = devfs_umount;
    devfs_ops.open    = devfs_open;
    devfs_ops.stat    = devfs_stat;
    devfs_ops.read    = devfs_read;
    devfs_ops.readdir = devfs_readdir;
    devfs_ops.write   = devfs_write;
    devfs_ops.create  = NULL;
    devfs_ops.unlink  = NULL;
    vfs_register_fs("devfs", &devfs_ops);
}
