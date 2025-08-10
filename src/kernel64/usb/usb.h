#pragma once
#include <stdint.h>

// Bring up basic USB host controllers and print findings.
// TODO: implement UHCI/OHCI/EHCI/xHCI and MSC BOT/UAS.
void usb_init(void);
