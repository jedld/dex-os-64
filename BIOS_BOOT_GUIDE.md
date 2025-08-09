# Non-UEFI (Legacy BIOS) Boot Support

This guide covers testing and using dex-os-64 on non-UEFI systems (legacy BIOS boot).

## Quick Test

```bash
# Test legacy BIOS boot in QEMU
./scripts/run_qemu_bios.sh

# Test UEFI boot in QEMU (for comparison)
./scripts/run_qemu.sh
```

## Boot Menu Options

When you boot the ISO, GRUB will auto-detect the boot mode and show:

1. **"dex-os-64 (BIOS/Legacy compatible)"** ← **Primary option, works everywhere**
2. **"dex-os-64 (UEFI native)"** ← Only available on UEFI systems
3. **"dex-os-64 (Alternative kernel)"** ← If kernel.elf is present

**Default**: Option 1 (BIOS/Legacy compatible) - this works on both UEFI and BIOS systems.

## How It Works

### Legacy BIOS Boot Path
```
BIOS → GRUB (SeaBIOS) → loader32.elf → kernel64.bin
```

1. **BIOS** loads GRUB from the ISO's boot sector
2. **GRUB** runs in 16/32-bit mode, loads `loader32.elf`
3. **loader32.elf** (Multiboot2):
   - Sets up GDT and paging (identity-mapped 4GB)
   - Enables x86_64 long mode
   - Loads `kernel64.bin` as a module
   - Jumps to 64-bit kernel entry point
4. **kernel64** runs in full 64-bit mode

### UEFI Boot Path (for comparison)
```
UEFI → BOOTX64.EFI → (same loader32 + kernel64)
```

## Testing Different Boot Methods

### 1. QEMU Legacy BIOS
```bash
# Explicit legacy BIOS test
./scripts/run_qemu_bios.sh

# With debugging
DEBUG_FLAGS="int,guest_errors,cpu_reset" ./scripts/run_qemu_bios.sh
```

### 2. QEMU UEFI
```bash
# UEFI test (needs OVMF)
./scripts/run_qemu.sh
```

### 3. VirtualBox (Auto-detects)
```bash
# Create hybrid ISO and use in VirtualBox
./scripts/make_iso.sh

# VM Settings:
# - Type: Linux 64-bit
# - Enable PAE/NX: ✅
# - Boot order: CD/DVD first
```

### 4. Real Hardware
```bash
# Flash to USB (works on both UEFI and BIOS PCs)
sudo dd if=build/hello-os.iso of=/dev/sdX bs=4M
```

## Compatibility Matrix

| System Type | Boot Method | Supported | Notes |
|-------------|-------------|-----------|--------|
| Modern UEFI PC | UEFI native | ✅ | Use BOOTX64.EFI option |
| Modern UEFI PC | UEFI → BIOS compat | ✅ | Use BIOS/Legacy option |
| Legacy BIOS PC | BIOS only | ✅ | Use BIOS/Legacy option |
| VirtualBox | Auto-detect | ✅ | Works with default settings |
| VMware | Auto-detect | ✅ | Should work (untested) |
| Hyper-V | UEFI/BIOS | ✅ | Should work (untested) |

## Troubleshooting BIOS Boot

### No Boot Menu Appears
```bash
# Check ISO has proper boot sector
file build/hello-os.iso
# Should show: "DOS/MBR boot sector"

# Check GRUB files are present
7z l build/hello-os.iso | grep -E "(boot/grub|loader32)"
```

### GRUB Loads But Kernel Fails
- **"Kernel panic"**: Memory issue, try more RAM
- **"Invalid opcode"**: CPU doesn't support x86_64
- **Black screen**: Try serial console output

### Serial Console
```bash
# QEMU automatically connects serial to stdio
./scripts/run_qemu_bios.sh

# Real hardware: connect serial cable to COM1 (115200 baud)
```

### Memory Detection Issues
Legacy BIOS systems may report memory differently:
- Basic memory info (640KB + extended)
- E820 memory map (modern BIOS)
- Check "Total RAM" in kernel output

## Hardware Requirements

### Minimum (Legacy BIOS)
- x86_64 CPU with long mode support
- 256 MB RAM minimum
- CD/DVD drive or USB boot capability

### CPU Feature Requirements
```bash
# Check CPU supports required features:
grep -E "(lm|nx|pae)" /proc/cpuinfo

# lm  = long mode (x86_64)
# nx  = No-execute bit
# pae = Physical Address Extension
```

## Build Options

### Standard Hybrid ISO (Recommended)
```bash
./scripts/make_iso.sh
# Creates: build/hello-os.iso (UEFI + BIOS)
```

### VirtualBox-Optimized
```bash
./scripts/make_iso_vbox.sh
# Creates: build/hello-os.iso (enhanced compatibility)
```

### Manual USB Setup
```bash
# For legacy BIOS systems that need manual setup
MOUNT=/mnt/usb ./scripts/make_usb.sh
```

## Debugging BIOS Boot

### QEMU Debug Logs
```bash
# Enable detailed logging
DEBUG_FLAGS="int,guest_errors,cpu_reset,mmu" ./scripts/run_qemu_bios.sh

# Check logs
tail -f build/qemu-*.log
tail -f build/serial-*.log
```

### Common Issues
1. **"GRUB rescue>"**: Boot files missing or corrupted
2. **"Error 15"**: File not found (check ISO contents)
3. **"Error 21"**: Selected disk does not exist
4. **Immediate reboot**: Kernel panic (check memory/CPU)

### Monitor Memory Layout
The kernel will show:
```
Memory map:
  0x0000000000000000 + 0x000000000009FC00  [USABLE]
  0x000000000009FC00 + 0x0000000000000400  [RESV]
  ...
Total RAM: 0x... bytes
```

This verifies memory detection is working correctly.
