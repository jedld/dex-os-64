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

if command -v grub-mkrescue >/dev/null 2>&1; then
  mkdir -p "${BUILD_DIR}"
  grub-mkrescue -o "${BUILD_DIR}/hello-os.iso" "${ISO_DIR}"
  echo "ISO created: ${BUILD_DIR}/hello-os.iso"
else
  echo "grub-mkrescue not found; skip ISO creation."
fi
