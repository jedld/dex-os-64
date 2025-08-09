#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

"${ROOT_DIR}/scripts/make_iso.sh"

OVMF_CODE=${OVMF_CODE:-}
OVMF_VARS=${OVMF_VARS:-}

# Probe common OVMF locations if not provided
if [[ -z "${OVMF_CODE}" ]]; then
  for c in \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd \
    /usr/share/edk2/x64/OVMF_CODE.fd; do
    [[ -f "$c" ]] && OVMF_CODE="$c" && break
  done
fi

if [[ -z "${OVMF_VARS}" ]]; then
  for v in \
    /usr/share/OVMF/OVMF_VARS.fd \
    /usr/share/OVMF/OVMF_VARS_4M.fd \
    /usr/share/edk2/ovmf/OVMF_VARS.fd \
    /usr/share/edk2/x64/OVMF_VARS.fd; do
    [[ -f "$v" ]] && OVMF_VARS="$v" && break
  done
fi
ISO_PATH="${BUILD_DIR}/hello-os.iso"

if [[ -z "${OVMF_CODE}" || ! -f "${OVMF_CODE}" ]]; then
  echo "OVMF_CODE.fd not found; set OVMF_CODE env var"
  exit 1
fi

# Copy OVMF_VARS to a writable location (QEMU writes firmware vars)
if [[ -z "${OVMF_VARS}" || ! -f "${OVMF_VARS}" ]]; then
  echo "OVMF_VARS.fd not found; set OVMF_VARS env var"
  exit 1
fi

WRITABLE_VARS="${BUILD_DIR}/OVMF_VARS.fd"
mkdir -p "${BUILD_DIR}"
cp -f "${OVMF_VARS}" "${WRITABLE_VARS}"
chmod u+rw "${WRITABLE_VARS}" || true

ACCEL_ARGS="-accel tcg"
if [[ "${KVM:-0}" == "1" ]]; then
  ACCEL_ARGS="-enable-kvm"
fi

qemu-system-x86_64 \
  ${ACCEL_ARGS} \
  -m 256M \
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
  -drive if=pflash,format=raw,file="${WRITABLE_VARS}" \
  -cdrom "${ISO_PATH}" \
  -boot d \
  -serial stdio \
  -no-reboot
