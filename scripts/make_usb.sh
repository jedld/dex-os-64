#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

MOUNT_DIR=${MOUNT:-}
if [[ -z "${MOUNT_DIR}" ]]; then
  echo "Set MOUNT=/mount/point to your FAT32 USB mount"
  exit 1
fi

"${ROOT_DIR}/scripts/build.sh"

sudo mkdir -p "${MOUNT_DIR}/EFI/BOOT" "${MOUNT_DIR}/boot/grub"
sudo cp -f "${BUILD_DIR}/stage/boot/grub/grub.cfg" "${MOUNT_DIR}/boot/grub/"
[ -f "${BUILD_DIR}/stage/EFI/BOOT/BOOTX64.EFI" ] && sudo cp -f "${BUILD_DIR}/stage/EFI/BOOT/BOOTX64.EFI" "${MOUNT_DIR}/EFI/BOOT/"
[ -f "${BUILD_DIR}/stage/EFI/BOOT/BOOTAA64.EFI" ] && sudo cp -f "${BUILD_DIR}/stage/EFI/BOOT/BOOTAA64.EFI" "${MOUNT_DIR}/EFI/BOOT/"
if [ -f "${BUILD_DIR}/stage/boot/kernel.elf" ]; then
  sudo cp -f "${BUILD_DIR}/stage/boot/kernel.elf" "${MOUNT_DIR}/boot/"
fi

echo "USB staged at ${MOUNT_DIR}"
