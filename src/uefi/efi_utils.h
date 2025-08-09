#pragma once
#include <stdarg.h>
#include <stdint.h>

typedef uint16_t CHAR16;

#if defined(__x86_64__)
#  if defined(__GNUC__)
#    define EFIAPI __attribute__((ms_abi))
#  else
#    define EFIAPI
#  endif
#else
#  define EFIAPI
#endif

typedef struct EFI_TABLE_HEADER {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct SIMPLE_TEXT_OUTPUT_INTERFACE SIMPLE_TEXT_OUTPUT_INTERFACE;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    void *OutputString;
    // ... not used
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

// Only the subset we need
typedef struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER Hdr;
    void *FirmwareVendor;
    uint32_t FirmwareRevision;
    void *ConsoleInHandle;
    void *ConIn;
    void *ConsoleOutHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut;
    void *StandardErrorHandle;
    SIMPLE_TEXT_OUTPUT_INTERFACE *StdErr;
    // ... rest unused here
} EFI_SYSTEM_TABLE;

typedef void *EFI_HANDLE;

typedef uint64_t EFI_STATUS;

// UEFI calling convention on x86_64 is Microsoft x64; GCC handles with default

static inline void efi_puts(EFI_SYSTEM_TABLE *SystemTable, const CHAR16 *s) {
    // OutputString: EFI_STATUS EFIAPI (*)(SIMPLE_TEXT_OUTPUT_INTERFACE*, CHAR16*)
    typedef EFI_STATUS (EFIAPI *OutputStringFn)(SIMPLE_TEXT_OUTPUT_INTERFACE*, const CHAR16*);
    OutputStringFn OutputString = (OutputStringFn)((EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL*)SystemTable->ConOut)->OutputString;
    OutputString((SIMPLE_TEXT_OUTPUT_INTERFACE*)SystemTable->ConOut, (const CHAR16*)s);
}

static inline void ascii_to_utf16(const char *ascii, CHAR16 *out) {
    while (*ascii) { *out++ = (unsigned char)(*ascii++); }
    *out = 0;
}
