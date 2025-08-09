#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR="${ROOT_DIR}/build"
"${ROOT_DIR}/scripts/make_iso.sh"
ISO_PATH="${BUILD_DIR}/hello-os.iso"
qemu-system-x86_64 \
  -accel tcg \
  -m 256M \
  -cdrom "${ISO_PATH}" \
  -boot d \
  -serial stdio \
  -no-reboot
