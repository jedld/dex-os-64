#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Build once and create ISO
./scripts/build.sh >/dev/null
./scripts/make_iso.sh >/dev/null

run_and_check() {
  local mode=$1
  local script=$2
  local timeout_s=${TIMEOUT_S:-25}

  echo "[INFO] Running ${mode} headless (timeout ${timeout_s}s)"
  # Run headless with timeout; do not fail the script if timeout kills QEMU
  TIMEOUT_S="${timeout_s}" timeout -k 2 "${timeout_s}s" env HEADLESS=1 NO_REBOOT=1 NO_SHUTDOWN=1 ${script} || true

  local log=$(ls -1 build/serial-*.log 2>/dev/null | tail -n 1 || true)
  if [[ -z "${log}" ]]; then
    echo "[FAIL] ${mode}: No serial log found in build/." >&2
    return 1
  fi

  # Required markers to consider boot successful
  local patterns=(
    "Selftest: 0x0123456789ABCDEF"
    "CPU vendor:"
    "CPU brand:"
    "Features ECX=0x"
    "Memory map:"
    "PMM/VMM initialized. Free:"
    "Entering shell. Type 'help'."
  )

  local fail=0
  for p in "${patterns[@]}"; do
    if ! grep -Fq "$p" "$log"; then
      echo "[FAIL] ${mode}: Missing marker: $p" >&2
      fail=1
    fi
  done

  if [[ $fail -ne 0 ]]; then
    echo "[FAIL] ${mode}: Smoke test failed. Log: $log" >&2
    return 1
  fi

  echo "[OK] ${mode}: Smoke test passed. Log: $log"
}

uefi_ok=0
bios_ok=0
run_and_check UEFI ./scripts/run_qemu.sh && uefi_ok=1 || uefi_ok=0
run_and_check BIOS ./scripts/run_qemu_bios.sh && bios_ok=1 || bios_ok=0

if [[ $uefi_ok -eq 1 && $bios_ok -eq 1 ]]; then
  echo "[OK] All smoke tests passed."
  exit 0
fi

echo "[FAIL] Smoke tests: UEFI=${uefi_ok}, BIOS=${bios_ok}"
exit 1
