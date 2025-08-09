#include "memtest.h"
#include "console.h"
#include <stddef.h>

static inline void write64(uint64_t addr, uint64_t val) {
    *(volatile uint64_t*)addr = val;
}
static inline uint64_t read64(uint64_t addr) {
    return *(volatile uint64_t*)addr;
}

// Simple patterns
static const uint64_t patterns[] = {
    0x0000000000000000ULL,
    0xFFFFFFFFFFFFFFFFULL,
    0xAAAAAAAAAAAAAAAAULL,
    0x5555555555555555ULL,
    0x0123456789ABCDEFULL,
    0xFEDCBA9876543210ULL,
};

static uint64_t run_patterns(uint64_t start, uint64_t len, int destructive) {
    uint64_t end = start + len;
    uint64_t errors = 0;

    // Save region if non-destructive (first pass)
    // For simplicity and safety, we only do destructive by default; non-destructive would require a buffer.
    (void)destructive;

    uint64_t n = (uint64_t)(sizeof(patterns)/sizeof(patterns[0]));
    for (uint64_t p = 0; p < n; ++p) {
        // Write pass with progress
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            write64(addr, patterns[p]);
            if ((i & 0xFFFF) == 0) {
                console_progress("Write pat", addr - start, len);
            }
        }
        console_progress("Write pat", len, len); console_putc('\r');
        // Verify pass with progress
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            uint64_t v = read64(addr);
            if (v != patterns[p]) {
                if (errors < 5) {
                    console_write("\nMISMATCH at 0x"); console_write_hex64(addr);
                    console_write(" read=0x"); console_write_hex64(v);
                    console_write(" expected=0x"); console_write_hex64(patterns[p]); console_write("\n");
                }
                errors++;
            }
            if ((i & 0xFFFF) == 0) {
                console_progress("Verify pat", addr - start, len);
            }
        }
        console_progress("Verify pat", len, len); console_write("\n");
    }

    // Walking 1s
    for (int bit = 0; bit < 64; ++bit) {
        uint64_t pat = 1ULL << bit;
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            write64(addr, pat);
            if ((i & 0xFFFF) == 0) console_progress("Walk1 write", addr - start, len);
        }
        console_progress("Walk1 write", len, len); console_putc('\r');
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            uint64_t v = read64(addr);
            if (v != pat) {
                if (errors < 5) {
                    console_write("\nMISMATCH at 0x"); console_write_hex64(addr);
                    console_write(" read=0x"); console_write_hex64(v);
                    console_write(" expected=0x"); console_write_hex64(pat); console_write("\n");
                }
                errors++;
            }
            if ((i & 0xFFFF) == 0) console_progress("Walk1 verify", addr - start, len);
        }
        console_progress("Walk1 verify", len, len); console_write("\n");
    }

    // Walking 0s
    for (int bit = 0; bit < 64; ++bit) {
        uint64_t pat = ~(1ULL << bit);
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            write64(addr, pat);
            if ((i & 0xFFFF) == 0) console_progress("Walk0 write", addr - start, len);
        }
        console_progress("Walk0 write", len, len); console_putc('\r');
        for (uint64_t addr = start, i = 0; addr + 8 <= end; addr += 8, ++i) {
            uint64_t v = read64(addr);
            if (v != pat) {
                if (errors < 5) {
                    console_write("\nMISMATCH at 0x"); console_write_hex64(addr);
                    console_write(" read=0x"); console_write_hex64(v);
                    console_write(" expected=0x"); console_write_hex64(pat); console_write("\n");
                }
                errors++;
            }
            if ((i & 0xFFFF) == 0) console_progress("Walk0 verify", addr - start, len);
        }
        console_progress("Walk0 verify", len, len); console_write("\n");
    }

    return errors;
}

uint64_t memtest_run(uint64_t start_phys, uint64_t length_bytes, int destructive) {
    console_write("Memtest region: start=0x"); console_write_hex64(start_phys);
    console_write(" length="); console_write_hex64(length_bytes); console_write(" bytes\n");
    uint64_t errs = run_patterns(start_phys, length_bytes, destructive);
    console_write("Memtest done. Errors: "); console_write_dec(errs); console_write("\n");
    return errs;
}

uint64_t memtest_quick(void) {
    // Avoid low 1 MiB and video memory; test 16 MiB starting at 16 MiB as a safe default
    uint64_t start = 16ULL * 1024 * 1024;
    uint64_t len   = 16ULL * 1024 * 1024;
    return memtest_run(start, len, 1);
}
