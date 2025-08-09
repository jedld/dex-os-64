# dex-os-64 — minimal "hello world" boot via UEFI and GRUB (USB)

This repo scaffolds a tiny, modern 64-bit OS starter that prints "Hello, world" using a UEFI application, for both x86_64 and AArch64. It includes:

- A CMake-based build (toolchain-friendly),
- UEFI apps for x86_64 and AArch64 that print to the firmware console,
- A GRUB config that chainloads the UEFI application when booted under GRUB on UEFI systems,
- Scripts to build, make an ISO, and stage a USB stick (FAT32) for UEFI boot.

Notes
- GRUB path here uses chainloading of the UEFI app (common on UEFI systems). A legacy BIOS-only GRUB path is not provided in this first cut.
- A bare-metal GRUB-multiboot kernel for x86_64 will be added in a follow-up step once the UEFI baseline is working on real hardware and QEMU.

## Build

Requirements (host Linux):
- CMake 3.20+
- GCC and binutils
- For x86_64 UEFI: no extra toolchain (host x86_64 gcc works) + binutils objcopy with efi-app-x86_64 support
- For AArch64 UEFI: aarch64-linux-gnu-gcc and aarch64-linux-gnu-objcopy
- Optional: grub-mkrescue, xorriso, mtools (for ISO), OVMF for QEMU UEFI runs

Quick build:

```bash
./scripts/build.sh
```

Artifacts:
- x86_64 UEFI app: `build/uefi/BOOTX64.EFI`
- AArch64 UEFI app: `build/uefi/BOOTAA64.EFI`

## Create UEFI USB (FAT32)

Prepare a USB stick with a single FAT32 partition and mount it (example mount: `/mnt/usb`). Then stage files:

```bash
MOUNT=/mnt/usb ./scripts/make_usb.sh
```

This installs:
- `EFI/BOOT/BOOTX64.EFI` (x86_64 UEFI app)
- `EFI/BOOT/BOOTAA64.EFI` (AArch64 UEFI app)
- `boot/grub/grub.cfg` (GRUB menu that chainloads the UEFI app on UEFI platforms)

Boot the USB in UEFI mode and you should see a firmware text console with: `Hello, world from UEFI!`.

## Make ISO (UEFI-friendly)

If you have `grub-mkrescue` and `xorriso`:

```bash
./scripts/make_iso.sh
```

Produces `build/hello-os.iso`. You can boot it via QEMU UEFI or some real machines. On UEFI systems, GRUB can chainload the UEFI app.

## Run in QEMU (UEFI)

If you have OVMF:

```bash
./scripts/run_qemu.sh
```

Adjust paths to your local OVMF binary if needed.

## Next steps

- Add a true GRUB Multiboot2 long-mode kernel for x86_64 (pure bare metal),
- Minimal VGA/serial text console, paging, and a linker script for long-mode,
- Early boot for AArch64 bare-metal (non-UEFI) on QEMU.

## License

MIT
