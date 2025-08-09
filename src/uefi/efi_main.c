#include <stdint.h>
#include "efi_utils.h"

// Minimal, portable UEFI entrypoint for both x86_64 and AArch64

#ifdef __clang__
#define USED __attribute__((used))
#else
#define USED __attribute__((used))
#endif

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) USED;

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
    static CHAR16 msg[64];
    ascii_to_utf16("Hello, world from UEFI!\r\n", msg);
    efi_puts(SystemTable, msg);
    return 0; // EFI_SUCCESS
}
