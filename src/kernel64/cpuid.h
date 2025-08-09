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
