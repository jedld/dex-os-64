#include "console.h"
#include "input.h"
#include "sched/sched.h"
#include "../kernel/mm/pmm.h"
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

static void shell_handle_line(char* buf, uint64_t n) {
    buf[n] = '\0';
    // skip leading spaces
    char* s = buf; while (*s == ' ' || *s == '\t') ++s;
    if (*s == '\0') return;
    // parse command
    char* cmd = s;
    while (*s && *s != ' ' && *s != '\t') ++s;
    char* args = s; while (*args == ' ' || *args == '\t') ++args;

    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "echo") == 0) {
        console_write(args);
        console_write("\n");
    } else if (strcmp(cmd, "clear") == 0) {
        console_clear();
    } else if (strcmp(cmd, "info") == 0) {
        show_info();
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
