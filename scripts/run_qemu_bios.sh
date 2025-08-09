#!/usr/bin/env bash
set -euo pipefail

# Run BIOS QEMU for the GRUB ISO (loader32 + kernel64), with optional no-reboot
# and automatic debug/serial logging.

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"

# Options (override via env)
# NO_REBOOT=1|0      -> add -no-reboot (default 1)
# NO_SHUTDOWN=1|0    -> add -no-shutdown (default 0)
# DEBUG_FLAGS=...    -> QEMU -d flags (default: int,guest_errors)
# DEBUG_LOG=path     -> QEMU -D log file (default: build/qemu-<timestamp>.log)
# SERIAL_LOG=path    -> Tee combined stdout/stderr to file (default: build/serial-<timestamp>.log)
# EXTRA_QEMU_ARGS=.. -> Any extra args appended to QEMU

NO_REBOOT=${NO_REBOOT:-1}
NO_SHUTDOWN=${NO_SHUTDOWN:-0}
DEBUG_FLAGS=${DEBUG_FLAGS:-"int,guest_errors"}
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
DEBUG_LOG=${DEBUG_LOG:-"${BUILD_DIR}/qemu-${TIMESTAMP}.log"}
SERIAL_LOG=${SERIAL_LOG:-"${BUILD_DIR}/serial-${TIMESTAMP}.log"}
EXTRA_QEMU_ARGS=${EXTRA_QEMU_ARGS:-}

mkdir -p "${BUILD_DIR}"

"${ROOT_DIR}/scripts/make_iso.sh"
ISO_PATH="${BUILD_DIR}/hello-os.iso"

QEMU=(
  qemu-system-x86_64
  -accel tcg
  -m 256M
  -cdrom "${ISO_PATH}"
  -boot d
  -serial stdio
  -d "${DEBUG_FLAGS}"
  -D "${DEBUG_LOG}"
)

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

# Capture serial/stdout/stderr to a file while showing it live
"${QEMU[@]}" 2>&1 | tee "${SERIAL_LOG}"
exit ${PIPESTATUS[0]}
