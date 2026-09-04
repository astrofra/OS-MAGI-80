#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_TOOLCHAIN_PREFIX="${MIGA80_TOOLCHAIN_PREFIX:-$HOME/.local/m68k-amigaos}"
MIGA80_PIPX_BIN="${MIGA80_PIPX_BIN:-$HOME/.local/bin}"
MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/miga80-amiga-tools-smoke.XXXXXX")"

trap 'rm -rf "$MIGA80_TEST_ROOT"' EXIT

export PATH="$MIGA80_PIPX_BIN:$MIGA80_TOOLCHAIN_PREFIX/bin:$PATH"

for command in hunktool m68k-amigaos-gcc vamos xdfscan xdftool; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

m68k-amigaos-gcc \
  -std=c99 \
  -m68020 \
  -msoft-float \
  -Wall \
  -Wextra \
  -Werror \
  -fno-common \
  "$MIGA80_PROJECT_ROOT/tests/smoke/c99-runtime/main.c" \
  -o "$MIGA80_TEST_ROOT/main"

# hunktool 0.8.1 returns the number of valid input files rather than zero.
# Validate its structured result instead of treating that count as failure.
hunktool validate "$MIGA80_TEST_ROOT/main" >"$MIGA80_TEST_ROOT/hunk-validation.txt" || true
if ! /usr/bin/grep -q 'TYPE_LOADSEG' "$MIGA80_TEST_ROOT/hunk-validation.txt"; then
  printf 'hunktool did not recognize the smoke test as a loadseg executable.\n' >&2
  exit 1
fi

MIGA80_VAMOS_OUTPUT="$(vamos -C 20 "$MIGA80_TEST_ROOT/main")"
if [ "$MIGA80_VAMOS_OUTPUT" != 'MIGA-80 C99/68020 smoke test passed.' ]; then
  printf 'Unexpected vamos output: %s\n' "$MIGA80_VAMOS_OUTPUT" >&2
  exit 1
fi

xdftool -f "$MIGA80_TEST_ROOT/miga80-ofs.adf" \
  create + format MIGA80OFS ofs \
  + write "$MIGA80_PROJECT_ROOT/tests/smoke/c99-runtime/main.c" SMOKE.C \
  + list >"$MIGA80_TEST_ROOT/ofs-list.txt"

xdftool -f "$MIGA80_TEST_ROOT/miga80-ffs.adf" \
  create + format MIGA80FFS ffs \
  + write "$MIGA80_PROJECT_ROOT/tests/smoke/c99-runtime/main.c" SMOKE.C \
  + list >"$MIGA80_TEST_ROOT/ffs-list.txt"

if ! /usr/bin/grep -q 'DOS0:ofs' "$MIGA80_TEST_ROOT/ofs-list.txt"; then
  printf 'xdftool did not create the expected OFS image.\n' >&2
  exit 1
fi

if ! /usr/bin/grep -q 'DOS1:ffs' "$MIGA80_TEST_ROOT/ffs-list.txt"; then
  printf 'xdftool did not create the expected FFS image.\n' >&2
  exit 1
fi

xdfscan "$MIGA80_TEST_ROOT/miga80-ofs.adf" >/dev/null
xdfscan "$MIGA80_TEST_ROOT/miga80-ffs.adf" >/dev/null

printf 'PASS  hunktool Amiga loadseg validation\n'
printf 'PASS  vamos 68020 execution and dos.library output\n'
printf 'PASS  xdftool OFS and FFS 880 KiB image creation\n'
printf 'PASS  xdfscan OFS and FFS filesystem validation\n'
