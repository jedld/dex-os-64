#include "../vfs/vfs.h"
#include "../console.h"
#include <stdint.h>
#include <stddef.h>
#include "../../kernel/mm/kmalloc.h"
#include "../serial.h"

// Simple root filesystem that just lists mount points as directories
// This is a stub - the real Unix-style path handling is done in the shell

typedef struct {
    int is_dir;
    char name[64];
    uint64_t size;
} rootfs_node_t;

// Build ops table at runtime
static vfs_fs_ops_t rootfs_ops;

static int rootfs_mount(block_device_t* bdev, const char* mname, void** out_priv) {
    (void)bdev; (void)mname;
    { const char* s="[rootfs] mount enter\n"; while(*s) serial_putc(*s++); }
    if (!out_priv) return -1;
    *out_priv = (void*)1; // dummy fs_priv
    { const char* s="[rootfs] mount exit\n"; while(*s) serial_putc(*s++); }
    return 0;
}

static void rootfs_umount(void* p){ (void)p; }

static int path_is_root(const char* sub){
    return (!sub) || (*sub=='\0') || ((*sub=='/'||*sub=='\0') && sub[1]=='\0');
}

static vfs_node_t* rootfs_open(void* fs_priv, const char* path){
    (void)fs_priv;
    
    rootfs_node_t* rn = (rootfs_node_t*)kmalloc(sizeof(rootfs_node_t));
    if (!rn) return NULL;
    vfs_node_t* vn = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!vn) { kfree(rn); return NULL; }
    
    vn->fops = &rootfs_ops; 
    vn->fs_priv = fs_priv; 
    vn->file_priv = rn;

    if (path_is_root(path)) {
        // Root directory
        rn->is_dir = 1;
        rn->name[0] = 0;
        rn->size = 0;
    } else {
        // For now, files in root don't exist
        kfree(vn);
        kfree(rn);
        return NULL;
    }
    
    return vn;
}

static int rootfs_stat(void* fs_priv, const char* path, uint64_t* size, int* is_dir){
    (void)fs_priv;
    
    if (path_is_root(path)) {
        if (size) *size = 0;
        if (is_dir) *is_dir = 1;
        return 0;
    }
    
    return -1; // File not found
}

static int rootfs_read(vfs_node_t* n, uint64_t off, void* buf, uint64_t len){
    (void)n; (void)off; (void)buf; (void)len;
    return -1; // No files to read in root fs
}

static int rootfs_readdir(vfs_node_t* n, uint32_t idx, char* name_out, uint32_t maxlen){
    if (!n || !name_out || maxlen == 0) return -1;
    rootfs_node_t* rn = (rootfs_node_t*)n->file_priv;
    
    if (!rn->is_dir) return -1;
    
    // Hardcoded list of mount points for now
    const char* mount_names[] = {"dev", "root"};
    const int mount_count = 2;
    
    if (idx >= (uint32_t)mount_count) return 0; // End of listing
    
    // Copy mount name
    int len = 0;
    const char* src = mount_names[idx];
    while (src[len] && len < (int)maxlen - 1) {
        name_out[len] = src[len];
        len++;
    }
    name_out[len] = 0;
    
    return 1; // Success
}

static int rootfs_write(vfs_node_t* n, uint64_t off, const void* buf, uint64_t len){
    (void)n; (void)off; (void)buf; (void)len;
    return -1; // No write support for rootfs
}

void rootfs_register(void){
    rootfs_ops.mount   = rootfs_mount;
    rootfs_ops.umount  = rootfs_umount;
    rootfs_ops.open    = rootfs_open;
    rootfs_ops.stat    = rootfs_stat;
    rootfs_ops.read    = rootfs_read;
    rootfs_ops.readdir = rootfs_readdir;
    rootfs_ops.write   = rootfs_write;
    rootfs_ops.create  = NULL;
    rootfs_ops.unlink  = NULL;
    vfs_register_fs("rootfs", &rootfs_ops);
}
