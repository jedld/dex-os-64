#pragma once
#include <stdint.h>
#include "../block/block.h"

// Modern, lightweight VFS core (single-threaded kernel for now).
// Path format: fs://mount_name/path or just / when default mount exists.

typedef struct vfs_node vfs_node_t;

typedef struct {
    int (*mount)(block_device_t* bdev, const char* mount_name, void** fs_priv);
    void (*umount)(void* fs_priv);
    // Open returns vfs_node with ops filled and fs_priv carried
    vfs_node_t* (*open)(void* fs_priv, const char* path);
    int (*stat)(void* fs_priv, const char* path, uint64_t* size, int* is_dir);
    int (*read)(vfs_node_t* node, uint64_t off, void* buf, uint64_t len);
    int (*readdir)(vfs_node_t* node, uint32_t index, char* name_out, uint32_t maxlen);
    // Optional write API and simple create/unlink on paths
    int (*write)(vfs_node_t* node, uint64_t off, const void* buf, uint64_t len);
    int (*create)(void* fs_priv, const char* path, uint64_t size_hint);
    int (*unlink)(void* fs_priv, const char* path);
} vfs_fs_ops_t;

struct vfs_node {
    const vfs_fs_ops_t* fops;
    void* fs_priv;
    void* file_priv; // per-open handle
};

int vfs_register_fs(const char* name, const vfs_fs_ops_t* ops);
int vfs_mount(const char* fs_name, const char* mount_name, const char* bdev_name);
int vfs_umount(const char* mount_name);

vfs_node_t* vfs_open(const char* path);
int vfs_read(vfs_node_t* n, uint64_t off, void* buf, uint64_t len);
int vfs_readdir(vfs_node_t* n, uint32_t idx, char* name, uint32_t maxlen);
int vfs_stat(const char* path, uint64_t* size, int* is_dir);
int vfs_write(vfs_node_t* n, uint64_t off, const void* buf, uint64_t len);
int vfs_create(const char* path, uint64_t size_hint);
int vfs_unlink(const char* path);

// Utility for shell
void vfs_list_mounts(void);
