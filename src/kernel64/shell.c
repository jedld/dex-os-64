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
    console_write("  ls [path]               - list directory\n");
    console_write("  cd <path>               - change directory\n");
    console_write("  pwd                     - print current directory\n");
    console_write("  mkexfat <dev> [label]   - format device as exFAT\n");
    console_write("  mkfs exfat <dev> [lbl]  - same as mkexfat\n");
    console_write("  cat <path>             - print file contents\n");
    console_write("  stat <path>            - show file size/type\n");
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
static char s_cwd_mnt[8] = {0};
static char s_cwd_path[128] = {0}; // without leading '/'

static int is_ws(char c){ return c==' '||c=='\t'; }
static void skip_ws(char** p){ while(is_ws(**p)) (*p)++; }

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
        if (fs[0]==0||mnt[0]==0||dev[0]==0) { console_write("usage: mount <fs> <mnt> <dev>\n"); }
        else { int rc=vfs_mount(fs,mnt,dev); if(rc!=0) console_write("mount failed\n"); }
    } else if (strcmp(cmd, "mounts") == 0) {
        vfs_list_mounts();
        // If no cwd, set cwd to first mount for convenience (not required)
        if (!s_cwd_mnt[0]) { /* no enumeration API yet */ }
    } else if (strcmp(cmd, "pwd") == 0) {
        if (s_cwd_mnt[0]) { console_write(s_cwd_mnt); console_write(":/"); console_write(s_cwd_path); console_putc('\n'); }
        else { console_write("(no cwd)\n"); }
    } else if (strcmp(cmd, "cd") == 0) {
        if (!*args) { console_write("usage: cd <path>\n"); }
        else {
            char full[192]; resolve_path(args, full, sizeof(full));
            if (!full[0]) { console_write("cd: set a mount with 'mount' first or use mount:path\n"); }
            else { set_cwd_from_path(full); }
        }
    } else if (strcmp(cmd, "ls") == 0) {
        char full[192];
        if (*args) resolve_path(args, full, sizeof(full)); else {
            if (!s_cwd_mnt[0]) { console_write("ls: no cwd. Use cd or specify mount:path\n"); return; }
            // build from cwd
            int p=0; for(; s_cwd_mnt[p] && p< (int)sizeof(full)-2; ++p) full[p]=s_cwd_mnt[p]; full[p++]=':'; full[p++]='/';
            for(int i=0; s_cwd_path[i] && p<(int)sizeof(full)-1; ++i) full[p++]=s_cwd_path[i]; full[p]=0;
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
            char full[192]; resolve_path(args, full, sizeof(full));
            vfs_node_t* n = vfs_open(full);
            if (!n) { console_write("cat: open failed\n"); }
            else {
                char buf[256]; uint64_t off=0; for(;;){ int r=vfs_read(n, off, buf, sizeof(buf)); if(r<=0) break; for(int i=0;i<r;++i) console_putc(buf[i]); off += (uint64_t)r; } console_putc('\n');
            }
        }
    } else if (strcmp(cmd, "stat") == 0) {
        if (!*args) { console_write("usage: stat <path>\n"); }
        else {
            char full[192]; resolve_path(args, full, sizeof(full));
            uint64_t sz=0; int isdir=0; int rc = vfs_stat(full, &sz, &isdir);
            if (rc!=0) { console_write("stat: not found\n"); }
            else { console_write(isdir?"dir":"file"); console_write(" size=0x"); console_write_hex64(sz); console_putc('\n'); }
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
