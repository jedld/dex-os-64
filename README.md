# dex-os-64 — x86_64 Operating System with Long Mode Support

An educational 64-bit operating system that boots via GRUB Multiboot2, enters x86_64 long mode, and provides a small kernel with text consoles, serial mirroring, a simple memory manager, and an interactive shell.

## Features

- Multiboot2 loader (32-bit) that enables long mode and jumps to the 64-bit kernel module
- x86_64 long mode kernel with proper SysV ABI calling conventions
- Text consoles:
   - VGA text 80x25 backend (0xB8000)
   - Multiboot2 EGA text framebuffer detection and binding (respects pitch/cols/rows)
   - Multiple console instances with offscreen buffer, scrolling, active-console switching
   - Serial mirroring of console output (COM1 @ 115200)
- CPU info via CPUID (vendor, brand, feature flags) with PIC-safe CPUID
- Memory info:
   - Parses EFI memory map or legacy Multiboot2 mmap; falls back to basic meminfo
   - Simple PMM (bitmap) and VMM identity mapping; early heap (kmalloc)
- Devices and shell:
   - PS/2 keyboard input
   - Display console device wrapper
   - Minimal VFS with devfs, RAM disk, and exFAT stubs; automatic root fs setup (devfs + ram0 exFAT) and interactive shell
- UEFI/BIOS hybrid ISO and QEMU run scripts with serial logging

## Architecture

The system consists of two main components:

1. **32-bit Loader** (`src/loader32/loader.S`):
   - Multiboot2-compliant entry point
   - Sets up GDT, paging (identity-mapped 1GB with 2MB pages)
   - Enables long mode and jumps to 64-bit kernel

2. **64-bit Kernel** (`src/kernel64/`):
   - Entry with proper stack and CPUID helpers
   - Console subsystem with multi-console text backends and serial mirroring
   - Memory map parsing from Multiboot2 (EFI mmap, legacy mmap, or basic meminfo)
   - PMM/VMM/early heap initialization and reservation of critical regions
   - Keyboard input (PS/2) and simple scheduler/shell

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

The kernel will display:
- CPU vendor and brand information
- CPU feature flags (ECX/EDX from CPUID)
- Complete memory map with usable and reserved regions
- Interactive prompt: "Press any key to continue..."

## What You'll See

When booted, the kernel displays system information:

```
dex-os-64 (x86_64)

CPU vendor: AuthenticAMD
CPU brand:  QEMU Virtual CPU version 2.5+
Features ECX=0x... EDX=0x...

Memory map:
  0x0000000000000000 + 0x000000000009FC00  [USABLE]
  0x000000000009FC00 + 0x0000000000000400  [RESV]
  0x00000000000E0000 + 0x0000000000020000  [RESV]
  ...
Total RAM: 0x... bytes
Reserved:  0x... bytes

PMM/VMM initialized. Free: 0x... bytes

Entering shell. Type 'help'.
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
│   ├── console.c/h    # Multi-console text backend (VGA or EGA text framebuffer)
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
- **Text console**: VGA text at 0xB8000, or EGA text framebuffer from MB2 tag (type=8)

### Key Features Implemented
- Multiboot2 specification compliance
- Long mode transition with proper GDT setup
- Identity paging (2MB pages for first 1GB)
- SysV ABI calling conventions
- Text consoles with cursor management, scrolling, and multi-instance support
- PS/2 keyboard polling
- Serial I/O for debugging and input
- Memory map parsing (EFI mmap preferred), PMM/VMM, and early heap
- CPU feature detection via CPUID

## Testing

Basic smoke test (build, run, assert key boot markers from the serial log):

```bash
chmod +x tests/smoke.sh
./tests/smoke.sh
```

This script builds the ISO, runs QEMU, then checks the latest `build/serial-*.log` for:
- Selftest hex print
- CPU vendor/brand and feature flags
- Memory map header and PMM/VMM free line
- Shell prompt marker

Note: The kernel now auto-mounts a RAM-backed root filesystem at boot:
- Mounts devfs as 'dev'
- Creates an 8MiB RAM disk 'ram0', formats it as exFAT, and mounts it as 'root'
- Use paths like `root:/` or `dev:/` in VFS-aware commands

Planned: add a lightweight unit/integration test harness that can run inside QEMU (headless) to validate subsystems (PMM/VMM, CPUID, console) automatically and fail on regressions.

## Development Notes

This kernel demonstrates several important OS development concepts:
- **Mode Transitions**: 32-bit to 64-bit long mode
- **Memory Management**: Basic paging and memory map interpretation  
- **I/O Operations**: Port I/O, VGA text or EGA text framebuffer, and serial
- **Hardware Abstraction**: CPU identification and feature detection
- **User Interaction**: Simple input handling for demonstration

## License

MIT
