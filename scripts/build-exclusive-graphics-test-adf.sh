#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

if [ "$#" -ne 4 ]; then
  printf 'Usage: %s program startup-sequence readme output.adf\n' "$0" >&2
  exit 1
fi

MAGI80_PROGRAM="$1"
MAGI80_STARTUP="$2"
MAGI80_README="$3"
MAGI80_ADF="$4"
MAGI80_MANIFEST="${MAGI80_ADF%.adf}.manifest.txt"
MAGI80_OUTPUT_DIR=$(dirname "$MAGI80_ADF")

for command in xdftool xdfscan; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

for input in "$MAGI80_PROGRAM" "$MAGI80_STARTUP" "$MAGI80_README"; do
  if [ ! -f "$input" ]; then
    printf 'Required ADF input not found: %s\n' "$input" >&2
    exit 1
  fi
done

/bin/mkdir -p "$MAGI80_OUTPUT_DIR"

xdftool -f "$MAGI80_ADF" \
  create \
  + format MAGI80BENCH ofs \
  + makedir S \
  + write "$MAGI80_STARTUP" S/Startup-Sequence \
  + write "$MAGI80_PROGRAM" BENCH \
  + write "$MAGI80_README" README.TXT \
  + boot install >/dev/null

xdfscan "$MAGI80_ADF" >/dev/null

MAGI80_ADF_SHA256=$(/usr/bin/shasum -a 256 "$MAGI80_ADF" | \
  /usr/bin/awk '{print $1}')
MAGI80_PROGRAM_SHA256=$(/usr/bin/shasum -a 256 "$MAGI80_PROGRAM" | \
  /usr/bin/awk '{print $1}')

{
  printf 'magi80_exclusive_graphics_test_adf_manifest=1\n'
  printf 'volume=MAGI80BENCH\n'
  printf 'filesystem=ofs\n'
  printf 'adf_bytes=%s\n' "$(/usr/bin/stat -f '%z' "$MAGI80_ADF")"
  printf 'adf_sha256=%s\n' "$MAGI80_ADF_SHA256"
  printf 'program_sha256=%s\n' "$MAGI80_PROGRAM_SHA256"
  printf 'benchmark_environment=physical_a1200_pal_candidate\n'
  printf 'timing_authority=real_hardware_candidate\n'
  printf 'report_path=MAGI80BENCH:RESULT.TXT\n'
  printf 'report_io=amigados_fwrite_setvbuf_32768\n'
  printf 'initial_report_footer=result=running\n'
  printf 'successful_report_footer=result=pass\n'
  printf '\nfilesystem_listing:\n'
  xdftool "$MAGI80_ADF" list
} >"$MAGI80_MANIFEST"

printf 'Built physical-test ADF: %s\n' "$MAGI80_ADF"
printf 'Wrote manifest: %s\n' "$MAGI80_MANIFEST"
