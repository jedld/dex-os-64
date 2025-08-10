#pragma once
#include <stdint.h>

typedef struct {
    uint8_t bus, dev, func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
} pci_device_t;

uint32_t pci_cfg_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint8_t  pci_cfg_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void     pci_cfg_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

typedef void (*pci_enum_cb)(const pci_device_t* dev, void* user);
void pci_enumerate(pci_enum_cb cb, void* user);
