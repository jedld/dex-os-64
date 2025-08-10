#include "vfs.h"
#include "../console.h"
#include "../serial.h"

static void dbg_puts(const char* s){ while(*s) serial_putc(*s++); serial_putc('\n'); }
static void dbg_put(const char* s){ while(*s) serial_putc(*s++); }
static void dbg_put_hex(uint32_t v){
    const char* hex = "0123456789ABCDEF";
    serial_putc('0'); serial_putc('x');
    for (int i=7;i>=0;--i){ uint8_t n=(v>>(i*4))&0xF; serial_putc(hex[n]); }
}
static void dbg_put_hex64(uint64_t v){
    const char* hex = "0123456789ABCDEF";
    serial_putc('0'); serial_putc('x');
    for (int i=15;i>=0;--i){ uint8_t n=(uint8_t)((v>>(i*4))&0xF); serial_putc(hex[n]); }
}
#include <stddef.h>

#define MAX_FS 4
#define MAX_MOUNTS 4

typedef struct { char name[8]; const vfs_fs_ops_t* ops; } fs_reg_t;
static fs_reg_t g_fs[MAX_FS]; static int g_fs_count = 0;

typedef struct { char mname[8]; const vfs_fs_ops_t* ops; void* fs_priv; } mount_t;
static mount_t g_mounts[MAX_MOUNTS]; static int g_mount_count = 0;

static int str_eq(const char* a, const char* b){while(*a&&*b&&*a==*b){++a;++b;}return *a==0&&*b==0;}
static void str_cpy(char* d, const char* s, int n){int i=0; for(;i<n-1&&s[i];++i)d[i]=s[i]; d[i]=0;}

int vfs_register_fs(const char* name, const vfs_fs_ops_t* ops){ if(g_fs_count>=MAX_FS) return -1; str_cpy(g_fs[g_fs_count].name,name,8); g_fs[g_fs_count].ops=ops; g_fs_count++; return 0; }
// Debug: report registrations
__attribute__((constructor)) static void vfs_dbg_init(void){ /* no-op to ensure file is linked */ }

int vfs_mount(const char* fs_name, const char* mount_name, const char* bdev_name){
    dbg_puts("[vfs] mount enter");
    dbg_put("[vfs]   fs_name="); dbg_put(fs_name?fs_name:"(null)"); serial_putc('\n');
    dbg_put("[vfs]   mount_name="); dbg_put(mount_name?mount_name:"(null)"); serial_putc('\n');
    dbg_put("[vfs]   bdev_name="); dbg_put((bdev_name&&bdev_name[0])?bdev_name:"(none)"); serial_putc('\n');
    if(g_mount_count>=MAX_MOUNTS) return -1;
    // find fs
    const vfs_fs_ops_t* fops=NULL; 
    for(int i=0;i<g_fs_count;++i){ 
        if(str_eq(g_fs[i].name,fs_name)){ 
            fops=g_fs[i].ops; 
            break; 
        }
    }
    dbg_put("[vfs]   fops="); dbg_put_hex64((uint64_t)(uintptr_t)fops); serial_putc('\n');
    if(!fops){ dbg_puts("[vfs]   fs not found"); return -1; }
    // find bdev (optional)
    block_device_t* bdev = NULL;
    if (bdev_name && bdev_name[0]) {
        bdev = block_find(bdev_name);
        dbg_put("[vfs]   bdev="); dbg_put_hex64((uint64_t)(uintptr_t)bdev); serial_putc('\n');
        if(!bdev) { dbg_puts("[vfs]   bdev not found"); return -1; }
    }
    void* priv=NULL; 
    int rc=0;
    dbg_put("[vfs]   fops->mount="); dbg_put_hex64((uint64_t)(uintptr_t)fops->mount); serial_putc('\n');
    dbg_puts("[vfs]   calling fops->mount...");
    if (!fops->mount) { dbg_puts("[vfs]   mount fn NULL, skipping"); priv=(void*)1; rc=0; goto mount_done; }
    rc=fops->mount(bdev,mount_name,&priv); 
    dbg_put("[vfs]   mount rc="); dbg_put_hex((uint32_t)rc); serial_putc('\n');
    dbg_put("[vfs]   fs_priv="); dbg_put_hex64((uint64_t)(uintptr_t)priv); serial_putc('\n');
    if(rc!=0) return rc; 
mount_done:
    // record mount
    str_cpy(g_mounts[g_mount_count].mname,mount_name,8); g_mounts[g_mount_count].ops=fops; g_mounts[g_mount_count].fs_priv=priv; g_mount_count++;
    dbg_puts("[vfs] mount exit");
    return 0;
}

int vfs_umount(const char* mount_name){ for(int i=0;i<g_mount_count;++i){ if(str_eq(g_mounts[i].mname,mount_name)){ if(g_mounts[i].ops->umount) g_mounts[i].ops->umount(g_mounts[i].fs_priv); // compact
            for(int j=i+1;j<g_mount_count;++j) g_mounts[j-1]=g_mounts[j]; g_mount_count--; return 0; }} return -1; }

static mount_t* find_mount(const char* mname){ for(int i=0;i<g_mount_count;++i) if(str_eq(g_mounts[i].mname,mname)) return &g_mounts[i]; return NULL; }

vfs_node_t* vfs_open(const char* path){
    // Expect form mount:/path or mount:path
    const char* p = path; const char* sep = path; while(*sep && *sep!=':') ++sep; if(*sep!=':') return NULL; char m[8]; int n= (sep-p<7? (int)(sep-p):7); for(int i=0;i<n;++i) m[i]=p[i]; m[n]=0; mount_t* mt=find_mount(m); if(!mt) return NULL; const char* sub = (*sep==':'? sep+1:sep);
    if(mt->ops->open) return mt->ops->open(mt->fs_priv, sub); return NULL;
}

int vfs_read(vfs_node_t* n, uint64_t off, void* buf, uint64_t len){ if(!n||!n->fops||!n->fops->read) return -1; return n->fops->read(n,off,buf,len); }
int vfs_readdir(vfs_node_t* n, uint32_t idx, char* name, uint32_t maxlen){ if(!n||!n->fops||!n->fops->readdir) return -1; return n->fops->readdir(n,idx,name,maxlen); }

int vfs_stat(const char* path, uint64_t* size, int* is_dir){
    // Expect form mount:/path or mount:path
    const char* p = path; const char* sep = path; while(*sep && *sep!=':') ++sep; if(*sep!=':') return -1; char m[8]; int n= (sep-p<7? (int)(sep-p):7); for(int i=0;i<n;++i) m[i]=p[i]; m[n]=0; mount_t* mt=find_mount(m); if(!mt) return -1; const char* sub = (*sep==':'? sep+1:sep);
    if(mt->ops->stat) return mt->ops->stat(mt->fs_priv, sub, size, is_dir); return -1;
}

void vfs_list_mounts(void){ console_write("Mounts:\n"); for(int i=0;i<g_mount_count;++i){ console_write("  "); console_write(g_mounts[i].mname); console_write("\n"); }}

static int parse_mount_and_sub(const char* path, mount_t** out_mt, const char** out_sub){
    const char* p = path; const char* sep = path; while(*sep && *sep!=':') ++sep; if(*sep!=':') return -1; char m[8]; int n= (sep-p<7? (int)(sep-p):7); for(int i=0;i<n;++i) m[i]=p[i]; m[n]=0; mount_t* mt=find_mount(m); if(!mt) return -1; const char* sub = (*sep==':'? sep+1:sep); *out_mt=mt; *out_sub=sub; return 0; }

int vfs_write(vfs_node_t* n, uint64_t off, const void* buf, uint64_t len){ if(!n||!n->fops||!n->fops->write) return -1; return n->fops->write(n,off,buf,len); }

int vfs_create(const char* path, uint64_t size_hint){ mount_t* mt; const char* sub; if(parse_mount_and_sub(path,&mt,&sub)!=0) return -1; if(!mt->ops->create) return -1; return mt->ops->create(mt->fs_priv, sub, size_hint); }

int vfs_unlink(const char* path){ mount_t* mt; const char* sub; if(parse_mount_and_sub(path,&mt,&sub)!=0) return -1; if(!mt->ops->unlink) return -1; return mt->ops->unlink(mt->fs_priv, sub); }
