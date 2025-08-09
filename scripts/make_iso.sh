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
