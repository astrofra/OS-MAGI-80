#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_TOOLCHAIN_PREFIX="${MIGA80_TOOLCHAIN_PREFIX:-$HOME/.local/m68k-amigaos}"
MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/miga80-toolchain-smoke.XXXXXX")"

trap 'rm -rf "$MIGA80_TEST_ROOT"' EXIT

export PATH="$MIGA80_TOOLCHAIN_PREFIX/bin:$PATH"

for command in m68k-amigaos-gcc m68k-amigaos-gdb m68k-amigaos-objdump m68k-amigaos-size vasmm68k_mot; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required target command not found: %s\n' "$command" >&2
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
  -ffunction-sections \
  -fdata-sections \
  "$MIGA80_PROJECT_ROOT/tests/smoke/c99-runtime/main.c" \
  -Wl,-Map,"$MIGA80_TEST_ROOT/main.map" \
  -o "$MIGA80_TEST_ROOT/main"

vasmm68k_mot \
  -quiet \
  -Fhunk \
  -m68020 \
  -o "$MIGA80_TEST_ROOT/minimal.o" \
  "$MIGA80_PROJECT_ROOT/tests/smoke/assembler/minimal.s"

if [ "$(m68k-amigaos-gcc -m68020 -msoft-float -print-multi-directory)" != "libm020" ]; then
  printf 'The compiler did not select the 68020 soft-float multilib.\n' >&2
  exit 1
fi

if ! /usr/bin/file "$MIGA80_TEST_ROOT/main" | /usr/bin/grep -q 'AmigaOS loadseg()ble'; then
  printf 'The smoke-test output is not an AmigaOS executable.\n' >&2
  exit 1
fi

if ! m68k-amigaos-objdump -f "$MIGA80_TEST_ROOT/main" | /usr/bin/grep -q 'file format amiga'; then
  printf 'Binutils did not recognize the smoke-test output as Amiga Hunk.\n' >&2
  exit 1
fi

if ! m68k-amigaos-objdump -f "$MIGA80_TEST_ROOT/minimal.o" | /usr/bin/grep -q 'file format amiga'; then
  printf 'VASM did not produce an Amiga Hunk object.\n' >&2
  exit 1
fi

if ! m68k-amigaos-gdb --batch -ex 'info files' "$MIGA80_TEST_ROOT/main" | /usr/bin/grep -q 'file type amiga'; then
  printf 'GDB could not load the Amiga Hunk executable.\n' >&2
  exit 1
fi

printf 'PASS  C99 compilation and AmigaOS Hunk link\n'
printf 'PASS  68020 soft-float multilib selection (libm020)\n'
printf 'PASS  AmigaOS API headers and dos.library calls\n'
printf 'PASS  VASM 68020 assembly and Amiga Hunk object output\n'
printf 'PASS  GDB Amiga Hunk executable loading\n'
m68k-amigaos-size "$MIGA80_TEST_ROOT/main"
