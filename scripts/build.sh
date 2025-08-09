#!/usr/bin/env bash
set -euo pipefail

# Build both x86_64 and aarch64 UEFI apps into build/uefi
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

if [[ "${CLEAN:-0}" == "1" ]]; then
  echo "Cleaning build directories..."
  rm -rf "${BUILD_DIR}/x86_64" "${BUILD_DIR}/aarch64"
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found. Please install cmake."
  exit 1
fi

if ! command -v clang >/dev/null 2>&1; then
  echo "clang not found. Please install clang (and lld)."
  exit 1
fi

if ! command -v ld.lld >/dev/null 2>&1; then
  echo "ld.lld not found. Please install lld."
  exit 1
fi

mkdir -p "${BUILD_DIR}/x86_64" "${BUILD_DIR}/aarch64"

GEN="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
  GEN="Unix Makefiles"
fi

# x86_64
(
  cd "${BUILD_DIR}/x86_64"
  if [ -f CMakeCache.txt ]; then
    cmake -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchains/x86_64-efi.cmake" "${ROOT_DIR}"
  else
    cmake -G "${GEN}" -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchains/x86_64-efi.cmake" "${ROOT_DIR}"
  fi
  cmake --build . --target uefi_x86_64
)

# aarch64
(
  cd "${BUILD_DIR}/aarch64"
  if [ -f CMakeCache.txt ]; then
    cmake -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchains/aarch64-efi.cmake" "${ROOT_DIR}"
  else
    cmake -G "${GEN}" -DCMAKE_TOOLCHAIN_FILE="${ROOT_DIR}/cmake/toolchains/aarch64-efi.cmake" "${ROOT_DIR}"
  fi
  cmake --build . --target uefi_aarch64
)

mkdir -p "${BUILD_DIR}/uefi"
if [ -f "${BUILD_DIR}/x86_64/bin/BOOTX64.EFI" ]; then
  cp -f "${BUILD_DIR}/x86_64/bin/BOOTX64.EFI" "${BUILD_DIR}/uefi/BOOTX64.EFI"
fi
if [ -f "${BUILD_DIR}/aarch64/bin/BOOTAA64.EFI" ]; then
  cp -f "${BUILD_DIR}/aarch64/bin/BOOTAA64.EFI" "${BUILD_DIR}/uefi/BOOTAA64.EFI"
fi

# Also stage GRUB config for packaging
mkdir -p "${BUILD_DIR}/stage/EFI/BOOT" "${BUILD_DIR}/stage/boot/grub"
[ -f "${BUILD_DIR}/uefi/BOOTX64.EFI" ] && cp -f "${BUILD_DIR}/uefi/BOOTX64.EFI" "${BUILD_DIR}/stage/EFI/BOOT/"
[ -f "${BUILD_DIR}/uefi/BOOTAA64.EFI" ] && cp -f "${BUILD_DIR}/uefi/BOOTAA64.EFI" "${BUILD_DIR}/stage/EFI/BOOT/"
cp -f "${ROOT_DIR}/boot/grub/grub.cfg" "${BUILD_DIR}/stage/boot/grub/"

echo "Built UEFI apps. Artifacts in ${BUILD_DIR}/uefi and staged tree in ${BUILD_DIR}/stage"
