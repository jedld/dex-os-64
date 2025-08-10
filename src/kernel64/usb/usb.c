#include "usb.h"
#include "../serial.h"
#include "../pci/pci.h"

static void log(const char* s){ while(*s) serial_putc(*s++); serial_putc('\n'); }

static void on_pci(const pci_device_t* d, void* user){ (void)user;
    // USB class = 0x0C, subclasses: 0x03 (UHCI), 0x10 (OHCI), 0x20 (EHCI), 0x30 (xHCI)
    if (d->class_code == 0x0C) {
        char msg[64];
        const char* type = "unknown";
        if (d->subclass == 0x03) type = "UHCI";
        else if (d->subclass == 0x10) type = "OHCI";
        else if (d->subclass == 0x20) type = "EHCI";
        else if (d->subclass == 0x30) type = "xHCI";
        log("[usb] controller found");
        // Future: init controller and enumerate devices
    }
}

void usb_init(void){
    log("[usb] init");
    pci_enumerate(on_pci, 0);
}
