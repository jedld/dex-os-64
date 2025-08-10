#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

"${ROOT_DIR}/scripts/make_iso.sh"

# Options (override via env)
# NO_REBOOT=1|0      -> add -no-reboot (default 1)
# NO_SHUTDOWN=1|0    -> add -no-shutdown (default 0)
# DEBUG_FLAGS=...    -> QEMU -d flags (default: int,guest_errors)
# DEBUG_LOG=path     -> QEMU -D log file (default: build/qemu-<timestamp>.log)
# SERIAL_LOG=path    -> Tee combined stdout/stderr to file (default: build/serial-<timestamp>.log)
# EXTRA_QEMU_ARGS=.. -> Any extra args appended to QEMU

NO_REBOOT=${NO_REBOOT:-1}
NO_SHUTDOWN=${NO_SHUTDOWN:-0}
HEADLESS=${HEADLESS:-0}
DEBUG_FLAGS=${DEBUG_FLAGS:-"int,guest_errors"}
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
DEBUG_LOG=${DEBUG_LOG:-"${BUILD_DIR}/qemu-${TIMESTAMP}.log"}
SERIAL_LOG=${SERIAL_LOG:-"${BUILD_DIR}/serial-${TIMESTAMP}.log"}
EXTRA_QEMU_ARGS=${EXTRA_QEMU_ARGS:-}

mkdir -p "${BUILD_DIR}"

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

QEMU=(
  qemu-system-x86_64
  ${ACCEL_ARGS}
  -smp ${SMP:-4}
  -m 256M
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}"
  -drive if=pflash,format=raw,file="${WRITABLE_VARS}"
  -cdrom "${ISO_PATH}"
  -boot d
  -serial stdio
  -d "${DEBUG_FLAGS}"
  -D "${DEBUG_LOG}"
)

if [[ "${HEADLESS}" == "1" ]]; then
  QEMU+=( -display none )
fi

if [[ "${NO_REBOOT}" == "1" ]]; then
  QEMU+=( -no-reboot )
fi
if [[ "${NO_SHUTDOWN}" == "1" ]]; then
  QEMU+=( -no-shutdown )
fi

if [[ -n "${EXTRA_QEMU_ARGS}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_ARR=( ${EXTRA_QEMU_ARGS} )
  QEMU+=( "${EXTRA_ARR[@]}" )
fi

echo "QEMU debug log: ${DEBUG_LOG}"
echo "Serial/stdout log: ${SERIAL_LOG}"

"${QEMU[@]}" 2>&1 | tee "${SERIAL_LOG}"
exit ${PIPESTATUS[0]}
