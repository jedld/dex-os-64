#include "vfs.h"
#include "../console.h"
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

int vfs_mount(const char* fs_name, const char* mount_name, const char* bdev_name){
    if(g_mount_count>=MAX_MOUNTS) return -1;
    // find fs
    const vfs_fs_ops_t* fops=NULL; for(int i=0;i<g_fs_count;++i){ if(str_eq(g_fs[i].name,fs_name)){ fops=g_fs[i].ops; break; }}
    if(!fops) return -1;
    // find bdev
    block_device_t* bdev = block_find(bdev_name);
    if(!bdev) return -1;
    void* priv=NULL; int rc=fops->mount(bdev,mount_name,&priv); if(rc!=0) return rc;
    str_cpy(g_mounts[g_mount_count].mname,mount_name,8); g_mounts[g_mount_count].ops=fops; g_mounts[g_mount_count].fs_priv=priv; g_mount_count++;
    console_write("mounted "); console_write(fs_name); console_write(" on "); console_write(mount_name); console_write("\n");
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
