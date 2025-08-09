// vmm.c — Virtual Memory Manager for x86_64 (4-level paging, 4KiB pages)
#include "vmm.h"
#include "pmm.h"
#include <stdint.h>
#include <stddef.h>

// Current PML4 physical address
static uint64_t g_cr3_phys = 0;

static inline void invlpg(uint64_t addr) {
    __asm__ volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

static inline uint64_t read_cr3(void) { uint64_t v; __asm__ volatile ("mov %%cr3,%0" : "=r"(v)); return v; }
static inline void write_cr3(uint64_t v) { __asm__ volatile ("mov %0,%%cr3" :: "r"(v) : "memory"); }

// Walk or create page-table levels; returns pointer to final PTE phys address slot
static uint64_t* get_pte(uint64_t pml4_phys, uint64_t va, int create) {
    uint64_t idx_pml4 = (va >> 39) & 0x1FF;
    uint64_t idx_pdp  = (va >> 30) & 0x1FF;
    uint64_t idx_pd   = (va >> 21) & 0x1FF;
    uint64_t idx_pt   = (va >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(uintptr_t)pml4_phys; // identity mapped early
    uint64_t pml4e = pml4[idx_pml4];
    if (!(pml4e & VMM_PRESENT)) {
        if (!create) return NULL;
        uint64_t newp = pmm_alloc_frames(1);
        if (!newp) return NULL;
        // zero new table
        uint64_t* tbl = (uint64_t*)(uintptr_t)newp;
        for (int i = 0; i < 512; ++i) tbl[i] = 0;
        pml4e = newp | VMM_PRESENT | VMM_RW;
        pml4[idx_pml4] = pml4e;
    }
    uint64_t* pdpt = (uint64_t*)(uintptr_t)(pml4e & ~0xFFFULL);
    uint64_t pdpte = pdpt[idx_pdp];
    if (!(pdpte & VMM_PRESENT)) {
        if (!create) return NULL;
        uint64_t newp = pmm_alloc_frames(1);
        if (!newp) return NULL;
        uint64_t* tbl = (uint64_t*)(uintptr_t)newp;
        for (int i = 0; i < 512; ++i) tbl[i] = 0;
        pdpte = newp | VMM_PRESENT | VMM_RW;
        pdpt[idx_pdp] = pdpte;
    }
    uint64_t* pd = (uint64_t*)(uintptr_t)(pdpte & ~0xFFFULL);
    uint64_t pde = pd[idx_pd];
    if (!(pde & VMM_PRESENT)) {
        if (!create) return NULL;
        uint64_t newp = pmm_alloc_frames(1);
        if (!newp) return NULL;
        uint64_t* tbl = (uint64_t*)(uintptr_t)newp;
        for (int i = 0; i < 512; ++i) tbl[i] = 0;
        pde = newp | VMM_PRESENT | VMM_RW;
        pd[idx_pd] = pde;
    }
    uint64_t* pt = (uint64_t*)(uintptr_t)(pde & ~0xFFFULL);
    return &pt[idx_pt];
}

void vmm_load_cr3(uint64_t pml4_phys) {
    g_cr3_phys = pml4_phys;
    write_cr3(pml4_phys);
}

void vmm_init_identity(void) {
    // Build a minimal PML4 and identity map the low 1 GiB with 4KiB pages for flexibility
    uint64_t pml4 = pmm_alloc_frames(1);
    if (!pml4) return;
    // zero
    for (int i = 0; i < 512; ++i) ((uint64_t*)(uintptr_t)pml4)[i] = 0;

    // Identity map first, say, 1GiB (0..0x3FFFFFFF)
    uint64_t flags = VMM_PRESENT | VMM_RW;
    for (uint64_t a = 0; a < (1ULL<<30); a += PAGE_SIZE) {
        uint64_t* pte = get_pte(pml4, a, 1);
        if (!pte) break;
        *pte = (a & ~0xFFFULL) | flags;
    }
    vmm_load_cr3(pml4);
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!g_cr3_phys) return -1;
    uint64_t* pte = get_pte(g_cr3_phys, virt, 1);
    if (!pte) return -2;
    *pte = (phys & ~0xFFFULL) | (flags & ~VMM_PS);
    invlpg(virt);
    return 0;
}

int vmm_unmap_page(uint64_t virt) {
    if (!g_cr3_phys) return -1;
    uint64_t* pte = get_pte(g_cr3_phys, virt, 0);
    if (!pte) return -2;
    *pte = 0;
    invlpg(virt);
    return 0;
}

int vmm_virt_to_phys(uint64_t virt, uint64_t* out_phys) {
    if (!g_cr3_phys || !out_phys) return 0;
    uint64_t* pte = get_pte(g_cr3_phys, virt, 0);
    if (!pte) return 0;
    uint64_t e = *pte;
    if (!(e & VMM_PRESENT)) return 0;
    *out_phys = (e & ~0xFFFULL) | (virt & 0xFFFULL);
    return 1;
}
