# dex-os-64 — x86_64 Operating System with Long Mode Support

A minimal 64-bit operating system that boots via GRUB Multiboot2, enters x86_64 long mode, and provides basic system information display with interactive keyboard support.

## Features

- **Multiboot2 Loader**: 32-bit loader that sets up long mode and jumps to 64-bit kernel
- **x86_64 Long Mode**: Full 64-bit kernel execution with proper calling conventions
- **VGA Text Console**: 80x25 color text output to VGA memory (0xB8000)
- **CPU Information Display**: Shows CPU vendor, brand string, and feature flags via CPUID
- **Memory Map**: Parses and displays Multiboot2 memory map with usable/reserved regions
- **Interactive Keyboard Support**: Waits for keypress (PS/2 keyboard or serial console)
- **Serial Console Support**: Dual input support for both PS/2 and serial console interaction
- **UEFI Boot Support**: Boots on UEFI systems via GRUB

## Architecture

The system consists of two main components:

1. **32-bit Loader** (`src/loader32/loader.S`):
   - Multiboot2-compliant entry point
   - Sets up GDT, paging (identity-mapped 1GB with 2MB pages)
   - Enables long mode and jumps to 64-bit kernel

2. **64-bit Kernel** (`src/kernel64/`):
   - Entry point with proper stack alignment
   - VGA console initialization and text output
   - CPU information extraction via CPUID
   - Memory map parsing from Multiboot2 info
   - Keyboard input handling (PS/2 + serial)

## Build Requirements

- CMake 3.20+
- Clang/LLVM toolchain
- Linux development environment
- GRUB tools (`grub-mkrescue`, `xorriso`) for ISO creation
- QEMU with OVMF for testing

## Quick Start

Build the system:
```bash
./scripts/build.sh
```

Create bootable ISO:
```bash
./scripts/make_iso.sh
```

Run in QEMU:
```bash
./scripts/run_qemu.sh
```

The system will display:
- CPU vendor and brand information
- CPU feature flags (ECX/EDX from CPUID)
- Complete memory map with usable and reserved regions
- Interactive prompt: "Press any key to continue..."

## What You'll See

When booted, the kernel displays system information:

```
dex-os-64 (x86_64)

CPU vendor: GenuineIntel
CPU brand:  Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz
Features ECX=0x... EDX=0x...

Memory map:
  0x0000000000000000 + 0x000000000009FC00  [USABLE]
  0x000000000009FC00 + 0x0000000000000400  [RESV]
  0x00000000000E0000 + 0x0000000000020000  [RESV]
  ...
Total RAM: 0x... bytes
Reserved:  0x... bytes

Done.
Press any key to continue...
```

The system waits for input from either:
- PS/2 keyboard (physical keyboard)
- Serial console (when using `-serial stdio` in QEMU)

## File Structure

```
src/
├── loader32/          # 32-bit Multiboot2 loader
│   ├── loader.S       # Assembly loader with long mode setup
│   └── CMakeLists.txt
├── kernel64/          # 64-bit kernel
│   ├── kmain64.c      # Main kernel with CPU info and memory map
│   ├── start64.S      # 64-bit entry point
│   ├── console.c/h    # VGA text console
│   ├── cpuid.h        # CPU identification
│   ├── io.h           # Port I/O operations
│   ├── mb2.h          # Multiboot2 structures
│   ├── serial.h       # Serial console support
│   ├── linker.ld      # Kernel linker script
│   └── CMakeLists.txt
├── kernel/            # Legacy kernel (unused)
└── uefi/              # UEFI applications
```

## Technical Details

### Boot Process
1. GRUB loads `loader32.elf` as Multiboot2 kernel
2. GRUB loads `kernel64.bin` as a module
3. Loader sets up minimal GDT and enables long mode
4. Loader parses Multiboot2 tags to find kernel64 module
5. Loader jumps to kernel64 entry point with Multiboot2 info in RDI
6. Kernel64 displays system information and waits for user input

### Memory Layout
- **Loader**: 32-bit code, identity-mapped first 1GB
- **Kernel**: 64-bit flat binary loaded as module
- **Stack**: 64-bit stack with proper SysV ABI alignment
- **VGA**: Text mode buffer at 0xB8000

### Key Features Implemented
- Multiboot2 specification compliance
- Long mode transition with proper GDT setup
- Identity paging (2MB pages for first 1GB)
- SysV ABI calling conventions
- VGA text console with cursor management
- PS/2 keyboard polling
- Serial I/O for debugging and input
- Robust memory map parsing
- CPU feature detection via CPUID

## Development Notes

This kernel demonstrates several important OS development concepts:
- **Mode Transitions**: 32-bit to 64-bit long mode
- **Memory Management**: Basic paging and memory map interpretation  
- **I/O Operations**: Both memory-mapped (VGA) and port-based (keyboard/serial)
- **Hardware Abstraction**: CPU identification and feature detection
- **User Interaction**: Simple input handling for demonstration

## License

MIT
