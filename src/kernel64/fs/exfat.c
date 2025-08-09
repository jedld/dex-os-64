#include "../vfs/vfs.h"
#include "../console.h"
#include <stdint.h>
#include <stddef.h>
#include "../../kernel/mm/kmalloc.h"
#include "../block/block.h"
#include "exfat.h"

// Minimal exFAT recognizer and very basic directory iteration (root only)
// This is a stub suitable for initial mount/ls; not a complete exFAT driver.

typedef struct { block_device_t* bdev; uint64_t fat_offset; uint64_t cluster_heap_off; uint32_t cluster_size; uint32_t root_dir_cluster; } exfat_fs_t;

typedef struct { exfat_fs_t* fs; uint32_t cluster; int is_dir; uint64_t size; } exfat_node_t;

static int bdev_read(block_device_t* b, uint64_t lba, void* buf, uint32_t sectors){ return b->ops->read(b,lba,buf,sectors); }

// Forward declare ops so exfat_open can assign node->fops
static const vfs_fs_ops_t exfat_ops;

static int exfat_mount(block_device_t* bdev, const char* mname, void** out_priv){ (void)mname; uint8_t sec[512]; if (bdev->sector_size != 512) { console_write("exfat: requires 512B sectors\n"); return -1; } if (bdev_read(bdev, 0, sec, 1) < 1) return -1; // Check signature at 0x1FE
    if (sec[510] != 0x55 || sec[511] != 0xAA) { console_write("exfat: bad vbr sig\n"); return -1; }
    // exFAT has signature "EXFAT   " at byte 3
    if (!(sec[3]=='E'&&sec[4]=='X'&&sec[5]=='F'&&sec[6]=='A'&&sec[7]=='T'&&sec[8]==' '&&sec[9]==' '&&sec[10]==' ')) {
        console_write("exfat: not exfat\n"); return -1;
    }
    exfat_fs_t* fs = (exfat_fs_t*)kmalloc(sizeof(exfat_fs_t)); if (!fs) return -1; fs->bdev=bdev;
    // Parse key fields from VBR (offsets per exFAT spec)
    uint8_t bytes_per_sector_shift = sec[0x0D];
    uint8_t sectors_per_cluster_shift = sec[0x0E];
    uint32_t fat_offset = *(uint32_t*)&sec[0x58];
    uint32_t cluster_heap_off = *(uint32_t*)&sec[0x60];
    uint32_t root_cluster = *(uint32_t*)&sec[0x70];
    fs->fat_offset = fat_offset;
    fs->cluster_heap_off = cluster_heap_off;
    fs->cluster_size = (1u << (bytes_per_sector_shift + sectors_per_cluster_shift));
    fs->root_dir_cluster = root_cluster;
    *out_priv = fs; return 0;
}

static void exfat_umount(void* p){ (void)p; }

static int path_is_root(const char* sub){ return (sub==NULL)||(*sub=='\0')||((*sub=='/')&&sub[1]=='\0'); }
static vfs_node_t* exfat_open(void* p, const char* path){ exfat_fs_t* fs=(exfat_fs_t*)p; exfat_node_t* n = (exfat_node_t*)kmalloc(sizeof(exfat_node_t)); if(!n) return NULL; n->fs=fs; vfs_node_t* vn=(vfs_node_t*)kmalloc(sizeof(vfs_node_t)); if(!vn) return NULL; vn->fops=&exfat_ops; vn->fs_priv=fs; vn->file_priv=n; 
    // Root path or empty -> open root dir
    if (!path || path_is_root(path)) { n->cluster=fs->root_dir_cluster; n->is_dir=1; n->size=0; return vn; }
    // Very simple demo: if path matches "/hello.txt" or "hello.txt" return a small virtual file of fixed size
    const char* q = path; if (*q=='/') ++q; const char* demo = "hello.txt"; int i=0; while (demo[i] && q[i] && demo[i]==q[i]) ++i; if (demo[i]==0 && (q[i]==0)) { n->cluster=0; n->is_dir=0; n->size=13; return vn; }
    // Otherwise not found
    return NULL; }

static int exfat_stat(void* p, const char* path, uint64_t* size, int* is_dir){ exfat_fs_t* fs=(exfat_fs_t*)p; (void)fs; if (!path || path_is_root(path)) { if(size) *size=0; if(is_dir) *is_dir=1; return 0; } const char* q=path; if(*q=='/') ++q; const char* demo="hello.txt"; int i=0; while(demo[i]&&q[i]&&demo[i]==q[i]) ++i; if(demo[i]==0 && q[i]==0){ if(size) *size=13; if(is_dir) *is_dir=0; return 0; } return -1; }
static int exfat_read(vfs_node_t* n, uint64_t off, void* buf, uint64_t len){ if(!n||!buf) return -1; exfat_node_t* en=(exfat_node_t*)n->file_priv; if(en->is_dir) return -1; const char* msg="Hello, world!"; uint64_t sz=13; if(off>=sz) return 0; uint64_t tocopy = (len > (sz-off) ? (sz-off) : len); unsigned char* d=(unsigned char*)buf; for(uint64_t i=0;i<tocopy;++i) d[i]=(unsigned char)msg[off+i]; return (int)tocopy; }
static int exfat_readdir(vfs_node_t* n, uint32_t idx, char* name_out, uint32_t maxlen){ if(!n||!name_out||maxlen==0) return -1; exfat_node_t* en=(exfat_node_t*)n->file_priv; if(!en->is_dir){ name_out[0]=0; return 0; } // root dir demo entries: . and hello.txt
    if(idx==0){ if(maxlen>1){ name_out[0]='.'; name_out[1]=0; } return 1; }
    if(idx==1){ const char* nm="hello.txt"; uint32_t i=0; for(; nm[i] && i<maxlen-1; ++i) name_out[i]=nm[i]; name_out[i]=0; return 1; }
    name_out[0]=0; return 0; }

static const vfs_fs_ops_t exfat_ops = { exfat_mount, exfat_umount, exfat_open, exfat_stat, exfat_read, exfat_readdir };

void exfat_register(void){ vfs_register_fs("exfat", &exfat_ops); }

// Very small exFAT "mkfs": create a plausible VBR to allow mounting.
// Not fully compliant; intended for demo/ramdisk only.
static void exfat_memset(void* d, int c, size_t n){ unsigned char* p=d; for(size_t i=0;i<n;++i)p[i]=(unsigned char)c; }
int exfat_format_device(const char* dev_name, const char* label_opt){
    block_device_t* b = block_find(dev_name);
    if (!b) return -1;
    if (b->sector_size != 512) return -1;
    // Build a simple VBR in memory
    uint8_t sec[512]; exfat_memset(sec, 0, sizeof(sec));
    // Jump + OEM name
    sec[0] = 0xEB; sec[1]=0x76; sec[2]=0x90; // short JMP
    const char* oem = "EXFAT   "; for (int i=0;i<8;i++) sec[3+i]=oem[i];
    // BIOS Parameter Block fields
    // bytes_per_sector_shift (assume 512 -> 9)
    sec[0x0D] = 9;
    // sectors_per_cluster_shift: choose 3 (8 sectors -> 4096B clusters)
    sec[0x0E] = 3;
    // FAT offset (in sectors)
    *(uint32_t*)&sec[0x58] = 24; // place FAT after reserved area
    // FAT length in sectors
    *(uint32_t*)&sec[0x5C] = 128;
    // cluster heap offset
    *(uint32_t*)&sec[0x60] = 24 + 128;
    // cluster count (rough estimate from device size)
    uint64_t total_bytes = (uint64_t)b->sector_count * b->sector_size;
    uint32_t cluster_size = (1u << (sec[0x0D] + sec[0x0E]));
    uint32_t clusters = (uint32_t)(total_bytes / cluster_size);
    if (clusters < 16) clusters = 16;
    *(uint32_t*)&sec[0x64] = clusters;
    // root dir start cluster (usually 2)
    *(uint32_t*)&sec[0x70] = 2;
    // volume label (optional), at offset 0x047 per spec is not fixed; we skip advanced fields
    // Signature
    sec[510] = 0x55; sec[511] = 0xAA;
    // Write VBR to LBA 0
    if (b->ops->write(b, 0, sec, 1) != 1) return -1;
    return 0;
}
