#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

if [ "$#" -ne 4 ]; then
  printf 'Usage: %s program startup-sequence readme output.adf\n' "$0" >&2
  exit 1
fi

MIGA80_PROGRAM="$1"
MIGA80_STARTUP="$2"
MIGA80_README="$3"
MIGA80_ADF="$4"
MIGA80_MANIFEST="${MIGA80_ADF%.adf}.manifest.txt"
MIGA80_OUTPUT_DIR=$(dirname "$MIGA80_ADF")

for command in xdftool xdfscan; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

for input in "$MIGA80_PROGRAM" "$MIGA80_STARTUP" "$MIGA80_README"; do
  if [ ! -f "$input" ]; then
    printf 'Required ADF input not found: %s\n' "$input" >&2
    exit 1
  fi
done

/bin/mkdir -p "$MIGA80_OUTPUT_DIR"

xdftool -f "$MIGA80_ADF" \
  create \
  + format MIGA80BENCH ofs \
  + makedir S \
  + write "$MIGA80_STARTUP" S/Startup-Sequence \
  + write "$MIGA80_PROGRAM" BENCH \
  + write "$MIGA80_README" README.TXT \
  + boot install >/dev/null

xdfscan "$MIGA80_ADF" >/dev/null

MIGA80_ADF_SHA256=$(/usr/bin/shasum -a 256 "$MIGA80_ADF" | \
  /usr/bin/awk '{print $1}')
MIGA80_PROGRAM_SHA256=$(/usr/bin/shasum -a 256 "$MIGA80_PROGRAM" | \
  /usr/bin/awk '{print $1}')

{
  printf 'miga80_exclusive_graphics_test_adf_manifest=1\n'
  printf 'volume=MIGA80BENCH\n'
  printf 'filesystem=ofs\n'
  printf 'adf_bytes=%s\n' "$(/usr/bin/stat -f '%z' "$MIGA80_ADF")"
  printf 'adf_sha256=%s\n' "$MIGA80_ADF_SHA256"
  printf 'program_sha256=%s\n' "$MIGA80_PROGRAM_SHA256"
  printf 'benchmark_environment=physical_a1200_pal_candidate\n'
  printf 'timing_authority=real_hardware_candidate\n'
  printf 'report_format=3\n'
  printf 'timing_source=cia_cascade_32\n'
  printf 'stock_case_count=204\n'
  printf 'stock_raw_result_count=120\n'
  printf 'stock_c2p_result_count=84\n'
  printf 'fast_assisted_case_count=56\n'
  printf 'fast_equipped_case_count=260\n'
  printf 'raw_core_kernel_count=6\n'
  printf 'raw_extended_kernel_count=26\n'
  printf 'raw_extended_dma_profile_count=3\n'
  printf 'fast_matrix_dma_profile_count=2\n'
  printf 'memory_traffic_fields=total_and_chip_lower_bounds\n'
  printf 'controlled_sprite_count=7\n'
  printf 'controlled_sprite_height=224\n'
  printf 'controlled_sprite_fetch_bytes_per_video_frame=6328\n'
  printf 'fair_blitter_copy_bytes=524288,2097152,4194176\n'
  printf 'hog_blitter_copy_bytes=524288\n'
  printf 'report_path=MIGA80BENCH:RESULT.TXT\n'
  printf 'report_io=amigados_fwrite_setvbuf_32768\n'
  printf 'initial_report_footer=result=running\n'
  printf 'successful_report_footer=result=pass\n'
  printf '\nfilesystem_listing:\n'
  xdftool "$MIGA80_ADF" list
} >"$MIGA80_MANIFEST"

printf 'Built physical-test ADF: %s\n' "$MIGA80_ADF"
printf 'Wrote manifest: %s\n' "$MIGA80_MANIFEST"
