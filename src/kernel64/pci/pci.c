// PCI config space access using legacy I/O ports CF8h/CFCh
#include "pci.h"
#include "../io.h"

static inline uint32_t cfg_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    return (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
}

uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    outl(0xCF8, cfg_addr(bus, dev, func, offset));
    return inl(0xCFC);
}

uint16_t pci_cfg_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, dev, func, offset & 0xFC);
    uint8_t sh = (offset & 2) ? 16 : 0;
    return (uint16_t)((v >> sh) & 0xFFFF);
}

uint8_t pci_cfg_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t v = pci_cfg_read32(bus, dev, func, offset & 0xFC);
    uint8_t sh = (offset & 3) * 8;
    return (uint8_t)((v >> sh) & 0xFF);
}

void pci_cfg_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    outl(0xCF8, cfg_addr(bus, dev, func, offset));
    outl(0xCFC, val);
}

void pci_enumerate(pci_enum_cb cb, void* user) {
    for (uint8_t bus = 0; bus < 255; ++bus) {
        for (uint8_t dev = 0; dev < 32; ++dev) {
            // Check function 0 first
            uint16_t vid0 = pci_cfg_read16(bus, dev, 0, 0x00);
            if (vid0 == 0xFFFF) continue; // no device present

            // Determine if multi-function
            uint8_t hdr = pci_cfg_read8(bus, dev, 0, 0x0E);
            uint8_t funcs = (hdr & 0x80) ? 8 : 1;

            for (uint8_t func = 0; func < funcs; ++func) {
                uint16_t vid = pci_cfg_read16(bus, dev, func, 0x00);
                if (vid == 0xFFFF) continue;

                pci_device_t d;
                d.bus = bus; d.dev = dev; d.func = func;
                d.vendor_id = vid;
                d.device_id = pci_cfg_read16(bus, dev, func, 0x02);
                d.class_code = pci_cfg_read8(bus, dev, func, 0x0B);
                d.subclass   = pci_cfg_read8(bus, dev, func, 0x0A);
                d.prog_if    = pci_cfg_read8(bus, dev, func, 0x09);
                if (cb) cb(&d, user);
            }
        }
    }
}
