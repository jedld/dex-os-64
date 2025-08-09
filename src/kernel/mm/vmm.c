#include "vmm.h"

// Placeholder VMM: stubs to be filled once long mode paging is active.

void vmm_init_identity(void) {
    // TODO: create a minimal identity map using PML4/PDPT/PD/PT and load CR3
}

int vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    (void)virt; (void)phys; (void)flags;
    // TODO: populate page tables
    return 0;
}

int vmm_unmap_page(uint64_t virt) {
    (void)virt;
    // TODO
    return 0;
}
