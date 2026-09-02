#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MAGI80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAGI80_OUTPUT_DIR="${MAGI80_FS_UAE_OUTPUT_DIR:-$MAGI80_PROJECT_ROOT/build/fs-uae}"

"$MAGI80_PROJECT_ROOT/scripts/configure-fs-uae.sh" >/dev/null

MAGI80_CONFIG_COUNT=0
for config_file in "$MAGI80_OUTPUT_DIR"/*.fs-uae; do
  if [ ! -f "$config_file" ]; then
    continue
  fi

  MAGI80_CONFIG_COUNT=$((MAGI80_CONFIG_COUNT + 1))
  /usr/bin/grep -qx 'amiga_model = A1200' "$config_file"
  /usr/bin/grep -qx 'chip_memory = 2048' "$config_file"
  /usr/bin/grep -qx 'fast_memory = 0' "$config_file"
  /usr/bin/grep -qx 'ntsc_mode = 0' "$config_file"
  /usr/bin/grep -q '^kickstart_file = /' "$config_file"

  if /usr/bin/grep -Eq 'A1200/020|fast_memory = [1-9]|zorro_iii_memory = [1-9]|jit_compiler = 1' "$config_file"; then
    printf 'Non-stock hardware option found in %s\n' "$config_file" >&2
    exit 1
  fi
  if /usr/bin/grep -Eq '@[A-Z0-9_]+@' "$config_file"; then
    printf 'Unresolved placeholder found in %s\n' "$config_file" >&2
    exit 1
  fi
done

if [ "$MAGI80_CONFIG_COUNT" -eq 0 ]; then
  printf 'No FS-UAE profile was generated.\n' >&2
  exit 1
fi

if [ -f "$MAGI80_OUTPUT_DIR/a1200-pal-ks30-rom.fs-uae" ]; then
  printf 'PASS  Kickstart 3.0/39.106 ROM validation\n'
else
  printf 'PENDING Kickstart 3.0/39.106 ROM is not configured\n'
fi
if [ -f "$MAGI80_OUTPUT_DIR/a1200-pal-ks31-rom.fs-uae" ]; then
  printf 'PASS  Kickstart 3.1/40.068 ROM validation\n'
else
  printf 'PENDING Kickstart 3.1/40.068 ROM is not configured\n'
fi

for profile in a1200-pal-ks30-hd a1200-pal-ks31-hd; do
  config_file="$MAGI80_OUTPUT_DIR/$profile.fs-uae"
  if [ ! -f "$config_file" ]; then
    continue
  fi
  /usr/bin/grep -q '^hard_drive_0 = /' "$config_file"
  /usr/bin/grep -qx 'hard_drive_0_read_only = 1' "$config_file"
  /usr/bin/grep -q '^hard_drive_1 = /' "$config_file"
  printf 'PASS  read-only AmigaOS system and writable staging drive in %s\n' "$profile"
done

for profile in a1200-pal-ks30-adf a1200-pal-ks31-adf; do
  config_file="$MAGI80_OUTPUT_DIR/$profile.fs-uae"
  if [ ! -f "$config_file" ]; then
    continue
  fi
  /usr/bin/grep -q '^floppy_drive_0 = /' "$config_file"
  printf 'PASS  generated floppy path in %s\n' "$profile"
done

printf 'PASS  stock PAL A1200 model, 2 MiB Chip RAM, zero Fast RAM\n'
printf 'PASS  generated FS-UAE configuration paths and placeholders\n'
