#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Build and run, relying on scripts to produce build/serial-*.log
./scripts/build.sh >/dev/null
./scripts/make_iso.sh >/dev/null
./scripts/run_qemu.sh >/dev/null || true

log=$(ls -1 build/serial-*.log 2>/dev/null | tail -n 1 || true)
if [[ -z "${log}" ]]; then
  echo "[FAIL] No serial log found in build/." >&2
  exit 1
fi

# Required markers to consider boot successful
patterns=(
  "Selftest: 0x0123456789ABCDEF"
  "CPU vendor:"
  "CPU brand:"
  "Features ECX=0x"
  "Memory map:"
  "PMM/VMM initialized. Free:"
  "Entering shell. Type 'help'."
)

fail=0
for p in "${patterns[@]}"; do
  if ! grep -Fq "$p" "$log"; then
    echo "[FAIL] Missing marker: $p" >&2
    fail=1
  fi
done

if [[ $fail -ne 0 ]]; then
  echo "[FAIL] Smoke test failed. Log: $log" >&2
  exit 1
fi

echo "[OK] Smoke test passed. Log: $log"
