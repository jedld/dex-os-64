#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

"${ROOT_DIR}/scripts/build.sh"

ISO_DIR="${BUILD_DIR}/iso_root"
rm -rf "${ISO_DIR}"
mkdir -p "${ISO_DIR}/EFI/BOOT" "${ISO_DIR}/boot/grub"
cp -a "${BUILD_DIR}/stage/EFI/BOOT/." "${ISO_DIR}/EFI/BOOT/" || true
cp -a "${BUILD_DIR}/stage/boot/grub/." "${ISO_DIR}/boot/grub/"
if [ -f "${BUILD_DIR}/stage/boot/kernel.elf" ]; then
  cp -f "${BUILD_DIR}/stage/boot/kernel.elf" "${ISO_DIR}/boot/" 
fi
if [ -f "${BUILD_DIR}/stage/boot/loader32.elf" ]; then
  cp -f "${BUILD_DIR}/stage/boot/loader32.elf" "${ISO_DIR}/boot/"
fi
if [ -f "${BUILD_DIR}/stage/boot/kernel64.bin" ]; then
  cp -f "${BUILD_DIR}/stage/boot/kernel64.bin" "${ISO_DIR}/boot/"
fi

# Create a simple root image (2 MiB). If mkfs.exfat is available, format it.
if [ ! -f "${ISO_DIR}/boot/root.img" ]; then
  dd if=/dev/zero of="${ISO_DIR}/boot/root.img" bs=1M count=2 status=none
fi

# Try to format as exFAT so the kernel can mount it as /root from the ISO.
if command -v mkfs.exfat >/dev/null 2>&1; then
  # Only (re)format if it doesn't already look like exFAT (cheap heuristic)
  if ! file "${ISO_DIR}/boot/root.img" | grep -qi 'exfat'; then
    mkfs.exfat -n ISOBOOT "${ISO_DIR}/boot/root.img" >/dev/null 2>&1 || true
  fi
  # If we can mount exFAT, add a sample file inside root.img for validation
  if command -v mount >/dev/null 2>&1 && command -v umount >/dev/null 2>&1; then
    TMPMNT="${BUILD_DIR}/mnt_rootimg"
    rm -rf "${TMPMNT}" && mkdir -p "${TMPMNT}"
    # Attempt to loop-mount; exfat kernel support or exfat-fuse must be present on host
    if sudo mount -o loop "${ISO_DIR}/boot/root.img" "${TMPMNT}" 2>/dev/null; then
      mkdir -p "${TMPMNT}/etc"
      echo "Hello from dex-os root.img at $(date -u +%Y-%m-%dT%H:%M:%SZ)" | sudo tee "${TMPMNT}/hello.txt" >/dev/null
      echo "dex-root=true" | sudo tee "${TMPMNT}/etc/os-release" >/dev/null
      sync || true
      sudo umount "${TMPMNT}" || true
    else
      echo "[make_iso] Could not loop-mount root.img (no exFAT mount support?). Skipping sample file."
    fi
  fi
else
  echo "[make_iso] mkfs.exfat not found; root.img will be blank and kernel will fall back to ram0."
fi

if command -v grub-mkrescue >/dev/null 2>&1; then
  mkdir -p "${BUILD_DIR}"
  
  # Create hybrid ISO with both UEFI and BIOS boot support
  # This ensures compatibility with both modern UEFI and legacy BIOS systems
  grub-mkrescue -o "${BUILD_DIR}/hello-os.iso" "${ISO_DIR}"
  
  echo "Hybrid ISO created (UEFI + BIOS): ${BUILD_DIR}/hello-os.iso"
  
  # Verify the ISO has both boot methods
  if command -v file >/dev/null 2>&1; then
    echo "ISO info:"
    file "${BUILD_DIR}/hello-os.iso"
  fi
else
  echo "grub-mkrescue not found; skip ISO creation."
fi
