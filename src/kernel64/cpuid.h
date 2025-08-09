#pragma once
#include <stdint.h>

typedef struct {
    uint32_t eax, ebx, ecx, edx;
} cpuid_regs;

static inline cpuid_regs cpuid(uint32_t leaf, uint32_t subleaf) {
    cpuid_regs r;
    __asm__ __volatile__("cpuid" : "=a"(r.eax), "=b"(r.ebx), "=c"(r.ecx), "=d"(r.edx)
                         : "a"(leaf), "c"(subleaf));
    return r;
}

// Initial APIC ID (legacy): CPUID leaf 1, EBX[31:24]
static inline uint32_t cpuid_initial_apic_id(void) {
    cpuid_regs r = cpuid(1, 0);
    return (r.ebx >> 24) & 0xFFu;
}

// Try to detect logical processor count (package-wide). Fallback to 1.
static inline uint32_t cpuid_logical_processor_count(void) {
    // Try leaf 0x0B extended topology
    cpuid_regs max = cpuid(0, 0);
    if (max.eax >= 0x0B) {
        cpuid_regs l = cpuid(0x0B, 0);
        uint32_t count = l.ebx & 0xFFFFu;
        if (count == 0) count = 1;
        return count;
    }
    // Fallback: leaf 1 EBX[23:16] is logical processors per package (legacy)
    if (max.eax >= 1) {
        cpuid_regs r1 = cpuid(1, 0);
        uint32_t count = (r1.ebx >> 16) & 0xFFu;
        if (count == 0) count = 1;
        return count;
    }
    return 1;
}
