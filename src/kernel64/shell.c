#include "console.h"
#include "input.h"
#include "sched/sched.h"
#include "../kernel/mm/pmm.h"
#include "vfs/vfs.h"
#include "block/block.h"
#include "fs/exfat.h"
int ramdisk_create(const char* name, uint64_t bytes);
#include <stddef.h>
#include <stdint.h>

static void cmd_help(void) {
    console_write("Built-in commands:\n");
    console_write("  help   - show this help\n");
    console_write("  echo X - echo text X\n");
    console_write("  info   - system info\n");
    console_write("  clear  - clear screen\n");
    console_write("  ps     - list threads\n");
    console_write("  mem    - memory totals (phys/free)\n");
    console_write("  free   - free memory bytes\n");
    console_write("  used   - used memory bytes\n");
    console_write("  mkram <name> <bytes_hex> - create RAM disk\n");
    console_write("  mount <fs> <mnt> <dev>   - mount device\n");
    console_write("  mounts                   - list mounts\n");
    console_write("  ls [path]               - list directory (Unix paths: /, /dev, /dev/ram0)\n");
    console_write("  cd <path>               - change directory (Unix paths: /, /dev, etc.)\n");
    console_write("  pwd                     - print current directory (Unix format)\n");
    console_write("  mkexfat <dev> [label]   - format device as exFAT\n");
    console_write("  mkfs exfat <dev> [lbl]  - same as mkexfat\n");
    console_write("  cat <path>             - print file contents\n");
    console_write("  stat <path>            - show file size/type\n");
    console_write("  touch <path>           - create empty file\n");
    console_write("  write <path> <text>    - write text to file (overwrite)\n");
    console_write("  rm <path>              - remove file\n");
    console_write("  fill <path> <size_hex> [ch] - write N bytes of ch (default 'A')\n");
        console_write("  demo                   - dots/dashes thread demo\n");
        console_write("  smp [N]                - spawn N worker threads\n");
        console_write("  memtest quick|range    - run memory tests\n");
}

static void show_info(void) {
    console_write("dex-os shell. Try 'help'.\n");
}

static int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void shell_prompt(void) {
    console_write("dex> ");
}

// Simple cwd state: mount and path within that mount
static char s_cwd_mnt[8] = { 'r','o','o','t', 0 };
static char s_cwd_path[128] = {0}; // without leading '/'

// Forward declarations for helper functions
static void str_copy(char* d, const char* s, int n);
static int str_len(const char* s);
static void resolve_path(const char* in, char* out, int outn);

// Unix-style path translation functions
int translate_unix_path(const char* unix_path, char* mount_path, int mount_path_len) {
    if (!unix_path || !mount_path || mount_path_len < 2) return -1;
    
    // Handle root directory specially
    if (unix_path[0] == '/' && unix_path[1] == 0) {
        // List root directory - use rootfs if mounted, otherwise fall back to current mount
        str_copy(mount_path, "root:/", mount_path_len);
        return 0;
    }
    
    if (unix_path[0] == '/') {
        // Absolute path: /mount/path or /mount or /path
        const char* p = unix_path + 1; // skip initial /
        
        // Extract first component
        char first_comp[16] = {0};
        int i = 0;
        while (p[i] && p[i] != '/' && i < 15) {
            first_comp[i] = p[i];
            i++;
        }
        first_comp[i] = 0;
        
        // Check if first component is a known mount point
        // We'll check for "dev" specifically for now, can be extended
        if (str_len(first_comp) > 0 && (
            strcmp(first_comp, "dev") == 0 ||
            strcmp(first_comp, "root") == 0)) {
            
            // This is a mount point, construct mount:path
            int pos = 0;
            // Copy mount name
            for (int j = 0; first_comp[j] && pos < mount_path_len - 1; j++) {
                mount_path[pos++] = first_comp[j];
            }
            if (pos < mount_path_len - 1) mount_path[pos++] = ':';
            
            // Copy rest of path
            const char* rest = p + i; // rest after mount name
            if (*rest == '/') rest++; // skip slash
            if (*rest == 0) {
                // Just the mount point itself, show root of that mount
                if (pos < mount_path_len - 1) mount_path[pos++] = '/';
            } else {
                // Copy the rest
                if (pos < mount_path_len - 1) mount_path[pos++] = '/';
                while (*rest && pos < mount_path_len - 1) {
                    mount_path[pos++] = *rest++;
                }
            }
            mount_path[pos] = 0;
            return 0;
        } else {
            // Not a mount point, treat as path in current/root mount
            if (s_cwd_mnt[0]) {
                // Use current mount
                int pos = 0;
                for (int j = 0; s_cwd_mnt[j] && pos < mount_path_len - 1; j++) {
                    mount_path[pos++] = s_cwd_mnt[j];
                }
                if (pos < mount_path_len - 1) mount_path[pos++] = ':';
                // Copy the path starting from the first component
                while (*unix_path && pos < mount_path_len - 1) {
                    mount_path[pos++] = *unix_path++;
                }
                mount_path[pos] = 0;
                return 0;
            } else {
                return -1; // No current mount
            }
        }
    } else {
        // Relative path - use existing resolve_path logic
        resolve_path(unix_path, mount_path, mount_path_len);
        return 0;
    }
}

static int is_ws(char c){ return c==' '||c=='\t'; }
static void skip_ws(char** p){ while(is_ws(**p)) (*p)++; }

// List available mount points for root directory listing
static void list_root_directory(void) {
    console_write("dev\n");   // devfs mount point  
    console_write("root\n");  // exfat mount point
    // Add more mount points here as they are added
}

static void str_copy(char* d, const char* s, int n){ int i=0; for(; i<n-1 && s[i]; ++i) d[i]=s[i]; d[i]=0; }
static int str_len(const char* s){ int n=0; while(s&&s[n])++n; return n; }

// Resolve a possibly relative path into mount:name form in out[...]
static void resolve_path(const char* in, char* out, int outn){
    // If in contains ':', assume mount:path
    const char* c = in; while(*c && *c!=':') ++c;
    if (*in && *c==':') {
        str_copy(out, in, outn);
        return;
    }
    // Else use cwd
    if (!s_cwd_mnt[0]) { out[0]=0; return; }
    char tmp[160]; int p=0;
    // mount:
    for (int i=0; s_cwd_mnt[i] && p<outn-1; ++i) tmp[p++]=s_cwd_mnt[i];
    if (p<outn-1) tmp[p++]=':';
    // base path
    if (in[0]=='/') {
        // absolute within mount
        int i=1; for(; in[i] && p<outn-1; ++i) tmp[p++]=in[i];
    } else {
        // relative
        int i=0; // start from cwd
        for (; s_cwd_path[i] && p<outn-1; ++i) tmp[p++]=s_cwd_path[i];
        if (p<outn-1) tmp[p++]='/';
        for (int j=0; in[j] && p<outn-1; ++j) tmp[p++]=in[j];
    }
    tmp[p]=0;
    // Normalize '.' and '..' segments
    char norm[192]; int np=0; int i=0;
    // copy mount:
    while(tmp[i] && tmp[i] != ':' && np<outn-1) norm[np++]=tmp[i++];
    if(tmp[i]==':' && np<outn-1) norm[np++]=tmp[i++];
    if (tmp[i] != '/') { if (np<outn-1) norm[np++]='/'; }
    // collect components into a small stack
    char comps[16][64]; int sp=0;
    while(tmp[i]){
        while(tmp[i]=='/') ++i; if(!tmp[i]) break;
        char comp[64]; int cj=0; while(tmp[i] && tmp[i]!='/'){ if(cj<63) comp[cj++]=tmp[i]; ++i; } comp[cj]=0;
        if (cj==0 || (comp[0]=='.' && comp[1]==0)) {
            // skip
        } else if (comp[0]=='.' && comp[1]=='.' && comp[2]==0) {
            if (sp>0) sp--;
        } else if (sp<16) {
            for(int k=0;k<64;++k) comps[sp][k]=0; for(int k=0;k<cj && k<63;++k) comps[sp][k]=comp[k]; sp++;
        }
    }
    for(int si=0; si<sp; ++si){ if(np<outn-1) norm[np++]='/'; int k=0; while(comps[si][k] && np<outn-1){ norm[np++]=comps[si][k++]; } }
    if (np==0 || norm[np-1]==':') { if(np<outn-1) norm[np++]='/'; }
    norm[np]=0;
    str_copy(out,norm,outn);
}

static void set_cwd_from_path(const char* path){
    // Expect mount:path
    const char* sep = path; while(*sep && *sep!=':') ++sep; if(*sep!=':') return;
    int n = (int)(sep - path); if (n>7) n=7; for(int i=0;i<n;++i) s_cwd_mnt[i]=path[i]; s_cwd_mnt[n]=0;
    // skip ':' and leading '/'
    const char* sub = sep+1; if (*sub=='/') ++sub;
    str_copy(s_cwd_path, sub, (int)sizeof(s_cwd_path));
}

static void shell_handle_line(char* buf, uint64_t n) {
    buf[n] = '\0';
    // skip leading spaces
    char* s = buf; skip_ws(&s);
    if (*s == '\0') return;
    // parse command
    char* cmd = s;
    while (*s && *s != ' ' && *s != '\t') ++s;
    char* end = s; // mark end of cmd token
    if (*end) { *end = '\0'; ++s; }
    char* args = s; skip_ws(&args);

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "echo") == 0) {
        console_write(args);
        console_write("\n");
    } else if (strcmp(cmd, "clear") == 0) {
        console_clear();
    } else if (strcmp(cmd, "info") == 0) {
        show_info();
    } else if (strcmp(cmd, "mkram") == 0) {
        // mkram <name> <size_hex>
        char name[16]={0}; uint64_t sz=0; {
            // parse name
            int i=0; skip_ws(&args); while(*args && !is_ws(*args) && i<15){ name[i++]=*args++; }
            skip_ws(&args);
            // parse hex size
            while (*args) {
                char c=*args++; uint8_t v;
                if (c>='0'&&c<='9') v=c-'0'; else if(c>='a'&&c<='f') v=10+(c-'a'); else if(c>='A'&&c<='F') v=10+(c-'A'); else break;
                sz = (sz<<4)|v;
            }
        }
        if (name[0]==0 || sz==0) { console_write("usage: mkram <name> <size_hex>\n"); }
        else { int rc = ramdisk_create(name, sz); if (rc!=0) console_write("mkram failed\n"); }
    } else if (strcmp(cmd, "mount") == 0) {
        // mount <fs> <mnt> <dev>
        char fs[8]={0}, mnt[8]={0}, dev[16]={0};
        // parse tokens
        char* a=args; skip_ws(&a); int i=0; while(*a && !is_ws(*a) && i<7) fs[i++]=*a++;
        skip_ws(&a); i=0; while(*a && !is_ws(*a) && i<7) mnt[i++]=*a++;
        skip_ws(&a); i=0; while(*a && !is_ws(*a) && i<15) dev[i++]=*a++;
        if (fs[0]==0||mnt[0]==0) { console_write("usage: mount <fs> <mnt> <dev?>\n"); }
        else {
            const char* devarg = (dev[0]?dev:"");
            int rc=vfs_mount(fs,mnt,devarg);
            if(rc!=0) console_write("mount failed\n");
        }
    } else if (strcmp(cmd, "mounts") == 0) {
        vfs_list_mounts();
        // If no cwd, set cwd to first mount for convenience (not required)
        if (!s_cwd_mnt[0]) { /* no enumeration API yet */ }
    } else if (strcmp(cmd, "pwd") == 0) {
        if (s_cwd_mnt[0]) { 
            // Convert to Unix-style path display
            if (strcmp(s_cwd_mnt, "dev") == 0) {
                console_write("/dev");
                if (s_cwd_path[0]) {
                    console_write("/");
                    console_write(s_cwd_path);
                }
            } else if (strcmp(s_cwd_mnt, "root") == 0) {
                if (s_cwd_path[0]) {
                    console_write("/");
                    console_write(s_cwd_path);
                } else {
                    console_write("/");
                }
            } else {
                // Fallback to old format for unknown mounts
                console_write(s_cwd_mnt); 
                console_write(":/"); 
                console_write(s_cwd_path);
            }
            console_putc('\n'); 
        }
        else { console_write("(no cwd)\n"); }
    } else if (strcmp(cmd, "cd") == 0) {
        if (!*args) { console_write("usage: cd <path>\n"); }
        else {
            char full[192];
            char unix_path[192];
            
            // Handle special shortcuts
            if (strcmp(args, "/") == 0) {
                // Go to root
                str_copy(s_cwd_mnt, "root", sizeof(s_cwd_mnt));
                s_cwd_path[0] = 0;
                return;
            }
            
            // Check if args looks like a mount name (no '/' or ':')
            int has_slash = 0, has_colon = 0;
            for (const char* p = args; *p; p++) {
                if (*p == '/') has_slash = 1;
                if (*p == ':') has_colon = 1;
            }
            
            if (!has_slash && !has_colon) {
                // Treat as mount name, add ":/" suffix
                int p = 0;
                for (int i = 0; args[i] && p < (int)sizeof(full) - 3; i++) {
                    full[p++] = args[i];
                }
                if (p < (int)sizeof(full) - 2) {
                    full[p++] = ':';
                    full[p++] = '/';
                }
                full[p] = 0;
            } else {
                // Handle Unix-style path
                if (args[0] == '/') {
                    str_copy(unix_path, args, sizeof(unix_path));
                } else {
                    // Relative path - construct current Unix path and resolve
                    char current_unix[192];
                    if (s_cwd_mnt[0]) {
                        if (strcmp(s_cwd_mnt, "dev") == 0) {
                            str_copy(current_unix, "/dev", sizeof(current_unix));
                            if (s_cwd_path[0]) {
                                int len = str_len(current_unix);
                                if (len < (int)sizeof(current_unix) - 1) {
                                    current_unix[len++] = '/';
                                    str_copy(current_unix + len, s_cwd_path, sizeof(current_unix) - len);
                                }
                            }
                        } else {
                            if (s_cwd_path[0]) {
                                current_unix[0] = '/';
                                str_copy(current_unix + 1, s_cwd_path, sizeof(current_unix) - 1);
                            } else {
                                str_copy(current_unix, "/", sizeof(current_unix));
                            }
                        }
                    } else {
                        str_copy(current_unix, "/", sizeof(current_unix));
                    }
                    
                    // Simple relative path resolution
                    int len = str_len(current_unix);
                    if (len < (int)sizeof(unix_path) - 1) {
                        str_copy(unix_path, current_unix, sizeof(unix_path));
                        if (unix_path[len-1] != '/') {
                            unix_path[len++] = '/';
                        }
                        str_copy(unix_path + len, args, sizeof(unix_path) - len);
                    }
                }
                
                // Translate Unix path to mount:path format
                if (translate_unix_path(unix_path, full, sizeof(full)) != 0) {
                    console_write("cd: path resolution failed\n");
                    return;
                }
            }
            
            if (!full[0]) { console_write("cd: set a mount with 'mount' first or use mount:path\n"); }
            else { set_cwd_from_path(full); }
        }
    } else if (strcmp(cmd, "ls") == 0) {
        char full[192];
        char unix_path[192];
        
        if (*args) {
            str_copy(unix_path, args, sizeof(unix_path));
        } else {
            // Use current directory - construct Unix path from cwd
            if (s_cwd_mnt[0]) {
                if (strcmp(s_cwd_mnt, "dev") == 0) {
                    str_copy(unix_path, "/dev", sizeof(unix_path));
                    if (s_cwd_path[0]) {
                        int len = str_len(unix_path);
                        if (len < (int)sizeof(unix_path) - 1) {
                            unix_path[len++] = '/';
                            str_copy(unix_path + len, s_cwd_path, sizeof(unix_path) - len);
                        }
                    }
                } else if (strcmp(s_cwd_mnt, "root") == 0) {
                    if (s_cwd_path[0]) {
                        unix_path[0] = '/';
                        str_copy(unix_path + 1, s_cwd_path, sizeof(unix_path) - 1);
                    } else {
                        str_copy(unix_path, "/", sizeof(unix_path));
                    }
                } else {
                    str_copy(unix_path, "/", sizeof(unix_path));
                }
            } else {
                str_copy(unix_path, "/", sizeof(unix_path));
            }
        }
        
        // Check for special case: root directory
        if (strcmp(unix_path, "/") == 0) {
            list_root_directory();
            return;
        }
        
        // Translate Unix path to mount:path format
        if (translate_unix_path(unix_path, full, sizeof(full)) != 0) {
            console_write("ls: path resolution failed\n");
            return;
        }
        
        vfs_node_t* n = vfs_open(full);
        if (!n) { console_write("ls: open failed\n"); }
        else {
            char name[64];
            for (uint32_t i=0; ; ++i) {
                int rc = vfs_readdir(n, i, name, sizeof(name));
                if (rc <= 0) break;
                console_write(name); console_putc('\n');
            }
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (!*args) { console_write("usage: cat <path>\n"); }
        else {
            char full[192];
            if (translate_unix_path(args, full, sizeof(full)) != 0) {
                // Fall back to old resolve_path for backward compatibility
                resolve_path(args, full, sizeof(full));
            }
            vfs_node_t* n = vfs_open(full);
            if (!n) { console_write("cat: open failed\n"); }
            else {
                char buf[256]; uint64_t off=0; for(;;){ int r=vfs_read(n, off, buf, sizeof(buf)); if(r<=0) break; for(int i=0;i<r;++i) console_putc(buf[i]); off += (uint64_t)r; } console_putc('\n');
            }
        }
    } else if (strcmp(cmd, "stat") == 0) {
        if (!*args) { console_write("usage: stat <path>\n"); }
        else {
            char full[192];
            if (translate_unix_path(args, full, sizeof(full)) != 0) {
                // Fall back to old resolve_path for backward compatibility
                resolve_path(args, full, sizeof(full));
            }
            uint64_t sz=0; int isdir=0; int rc = vfs_stat(full, &sz, &isdir);
            if (rc!=0) { console_write("stat: not found\n"); }
            else { console_write(isdir?"dir":"file"); console_write(" size=0x"); console_write_hex64(sz); console_putc('\n'); }
        }
    } else if (strcmp(cmd, "touch") == 0) {
        if (!*args) { console_write("usage: touch <path>\n"); }
        else { char full[192]; resolve_path(args, full, sizeof(full)); int rc=vfs_create(full,0); if(rc!=0) console_write("touch failed\n"); }
    } else if (strcmp(cmd, "write") == 0) {
        // write <path> <text>
        char* a=args; if(!*a){ console_write("usage: write <path> <text>\n"); }
        else {
            // parse path token
            char path[192]; int p=0; while(*a && !is_ws(*a) && p< (int)sizeof(path)-1){ path[p++]=*a++; } path[p]=0; skip_ws(&a);
            if (!path[0]) { console_write("usage: write <path> <text>\n"); }
            else {
                char full[192]; resolve_path(path, full, sizeof(full));
                vfs_node_t* n = vfs_open(full);
                if (!n) { // try create then reopen
                    if (vfs_create(full,0)!=0) { console_write("write: create failed\n"); }
                    n = vfs_open(full);
                }
                if (!n) { console_write("write: open failed\n"); }
                else {
                    // Overwrite from offset 0
                    int rc=vfs_write(n,0,a,(uint64_t)str_len(a)); if(rc<0) console_write("write failed\n"); else console_write("ok\n");
                }
            }
        }
    } else if (strcmp(cmd, "rm") == 0) {
        if (!*args) { console_write("usage: rm <path>\n"); }
        else { char full[192]; resolve_path(args, full, sizeof(full)); int rc=vfs_unlink(full); if(rc!=0) console_write("rm failed\n"); }
    } else if (strcmp(cmd, "fill") == 0) {
        // fill <path> <size_hex> [ch]
        char* a=args; if(!*a){ console_write("usage: fill <path> <size_hex> [ch]\n"); }
        else {
            char path[192]; int p=0; while(*a && !is_ws(*a) && p<(int)sizeof(path)-1){ path[p++]=*a++; } path[p]=0; skip_ws(&a);
            if(!path[0]) { console_write("usage: fill <path> <size_hex> [ch]\n"); }
            else {
                // parse hex size
                uint64_t sz=0; while(*a){ char c=*a++; int v; if(c>='0'&&c<='9') v=c-'0'; else if(c>='a'&&c<='f') v=10+(c-'a'); else if(c>='A'&&c<='F') v=10+(c-'A'); else break; sz=(sz<<4)|((uint64_t)v); }
                skip_ws(&a); char ch = (*a? *a : 'A');
                if(sz==0){ console_write("fill: size must be > 0\n"); }
                else {
                    char full[192]; resolve_path(path, full, sizeof(full));
                    vfs_node_t* n = vfs_open(full);
                    if(!n){ if(vfs_create(full,0)!=0){ console_write("fill: create failed\n"); }
                        n = vfs_open(full);
                    }
                    if(!n){ console_write("fill: open failed\n"); }
                    else {
                        uint8_t buf[512]; for(int i=0;i<512;++i) buf[i]=(uint8_t)ch;
                        uint64_t off=0; while(off<sz){ uint64_t towr = sz - off; if(towr>512) towr=512; int rc=vfs_write(n, off, buf, towr); if(rc<0){ console_write("fill: write error\n"); break; } off += (uint64_t)rc; }
                        if(off==sz) console_write("ok\n");
                    }
                }
            }
        }
    } else if (strcmp(cmd, "mkexfat") == 0) {
        // mkexfat <dev> [label]
        char dev[16]={0}; char label[16]={0};
        int i=0; skip_ws(&args); while(*args && !is_ws(*args) && i<15) dev[i++]=*args++;
        skip_ws(&args); i=0; while(*args && !is_ws(*args) && i<15) label[i++]=*args++;
        if (!dev[0]) { console_write("usage: mkexfat <dev> [label]\n"); }
        else {
            int rc = exfat_format_device(dev, label[0]?label:NULL);
            if (rc!=0) console_write("mkexfat failed\n"); else console_write("formatted exfat\n");
        }
    } else if (strcmp(cmd, "mkfs") == 0) {
        // mkfs exfat <dev> [label]
        char fs[8]={0}, dev[16]={0}, label[16]={0}; char* a=args; int i=0;
        skip_ws(&a); while(*a && !is_ws(*a) && i<7) fs[i++]=*a++;
        skip_ws(&a); i=0; while(*a && !is_ws(*a) && i<15) dev[i++]=*a++;
        skip_ws(&a); i=0; while(*a && !is_ws(*a) && i<15) label[i++]=*a++;
        if (fs[0]==0||dev[0]==0) { console_write("usage: mkfs exfat <dev> [label]\n"); }
        else if (strcmp(fs, "exfat")==0) {
            int rc = exfat_format_device(dev, label[0]?label:NULL);
            if (rc!=0) console_write("mkfs exfat failed\n"); else console_write("formatted exfat\n");
        } else { console_write("mkfs: unsupported fs\n"); }
    } else if (strcmp(cmd, "ps") == 0) {
        sched_thread_info_t ti[16];
        int cur = sched_current_id();
        int n = sched_enumerate(ti, 16);
        console_write("ID   STATE   RSP         CUR?\n");
        for (int i = 0; i < n; ++i) {
            console_write_dec((uint64_t)ti[i].id);
            console_write("   ");
            const char* st = (ti[i].state==0?"ready":(ti[i].state==1?"run":"done"));
            console_write(st);
            // pad
            if (ti[i].state==0) console_write("   "); else if (ti[i].state==1) console_write("    "); else console_write("   ");
            console_write("   0x"); console_write_hex64(ti[i].rsp);
            console_write((ti[i].id==cur)?"   *\n":"\n");
        }
    } else if (strcmp(cmd, "mem") == 0) {
        uint64_t total = pmm_total_physical_bytes();
        uint64_t freeb = pmm_free_bytes();
        console_write("Physical total: 0x"); console_write_hex64(total); console_write(" bytes\n");
        console_write("Free:           0x"); console_write_hex64(freeb); console_write(" bytes\n");
        if (total >= freeb) {
            uint64_t used = total - freeb;
            console_write("Used:           0x"); console_write_hex64(used); console_write(" bytes\n");
        }
    } else if (strcmp(cmd, "free") == 0) {
        console_write_hex64(pmm_free_bytes()); console_putc('\n');
    } else if (strcmp(cmd, "used") == 0) {
        uint64_t total = pmm_total_physical_bytes();
        uint64_t freeb = pmm_free_bytes();
        console_write_hex64(total >= freeb ? (total - freeb) : 0); console_putc('\n');
    } else {
        console_write("unknown command. type 'help'\n");
    }
}

void shell_main(void* _) {
    (void)_;
    console_write("\nEntering shell. Type 'help'.\n");
    char line[128];
    for (;;) {
        shell_prompt();
        uint64_t n = input_readline(line, sizeof(line));
        shell_handle_line(line, n);
        sched_yield();
    }
}
