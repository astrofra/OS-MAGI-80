#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MAGI80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAGI80_TOOLCHAIN_PREFIX="${MAGI80_TOOLCHAIN_PREFIX:-/Users/fra/.local/m68k-amigaos}"
MAGI80_PIPX_BIN="${MAGI80_PIPX_BIN:-/Users/fra/.local/bin}"
MAGI80_OUTPUT_DIR="${MAGI80_RUNTIME_OUTPUT_DIR:-$MAGI80_PROJECT_ROOT/build/runtime-comparison}"
MAGI80_RUN_FS_UAE="${MAGI80_RUNTIME_COMPARE_FS_UAE:-1}"

MAGI80_CC="$MAGI80_TOOLCHAIN_PREFIX/bin/m68k-amigaos-gcc"
MAGI80_NM="$MAGI80_TOOLCHAIN_PREFIX/bin/m68k-amigaos-nm"
MAGI80_OBJDUMP="$MAGI80_TOOLCHAIN_PREFIX/bin/m68k-amigaos-objdump"
MAGI80_SIZE="$MAGI80_TOOLCHAIN_PREFIX/bin/m68k-amigaos-size"
MAGI80_STRIP="$MAGI80_TOOLCHAIN_PREFIX/bin/m68k-amigaos-strip"
MAGI80_BOOTSTRAP_SOURCE="$MAGI80_PROJECT_ROOT/src/main.c"
MAGI80_MATRIX_SOURCE="$MAGI80_PROJECT_ROOT/tests/smoke/c-runtime-matrix/main.c"
MAGI80_BASELINE_TSV="$MAGI80_OUTPUT_DIR/baseline.tsv"
MAGI80_FUNCTIONAL_TSV="$MAGI80_OUTPUT_DIR/functional.tsv"
MAGI80_FS_UAE_MARKER="$MAGI80_PROJECT_ROOT/build/staging/fs-uae-smoke.out"

MAGI80_COMMON_FLAGS=(
  -std=c99
  -m68020
  -msoft-float
  -Os
  -Wall
  -Wextra
  -Werror
  -fno-common
  -ffunction-sections
  -fdata-sections
)

export PATH="$MAGI80_PIPX_BIN:$MAGI80_TOOLCHAIN_PREFIX/bin:$PATH"

restore_staged_program() {
  if [ -f "$MAGI80_PROJECT_ROOT/build/amiga/magi80" ]; then
    /bin/mkdir -p "$MAGI80_PROJECT_ROOT/build/staging"
    /bin/cp "$MAGI80_PROJECT_ROOT/build/amiga/magi80" \
      "$MAGI80_PROJECT_ROOT/build/staging/magi80"
    /bin/chmod 755 "$MAGI80_PROJECT_ROOT/build/staging/magi80"
  fi
}

trap restore_staged_program EXIT

for command in hunktool vamos; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done
for command in "$MAGI80_CC" "$MAGI80_NM" "$MAGI80_OBJDUMP" "$MAGI80_SIZE" "$MAGI80_STRIP"; do
  if [ ! -x "$command" ]; then
    printf 'Required target tool not found: %s\n' "$command" >&2
    exit 1
  fi
done

case "$MAGI80_RUN_FS_UAE" in
  0|1) ;;
  *)
    printf 'MAGI80_RUNTIME_COMPARE_FS_UAE must be 0 or 1.\n' >&2
    exit 1
    ;;
esac

build_binary() {
  local runtime="$1"
  local source_file="$2"
  local output_file="$3"
  local map_file="$4"
  local log_file="$5"
  local extra_define="$6"
  local extra_link_flag="$7"
  local selector="$8"
  local runtime_define="-DMAGI80_RUNTIME_NAME=\"$runtime\""
  local command=(
    "$MAGI80_CC"
    "${MAGI80_COMMON_FLAGS[@]}"
    "$runtime_define"
  )

  if [ -n "$extra_define" ]; then
    command+=("$extra_define")
  fi
  command+=(
    "$source_file"
    "-Wl,-Map,$map_file"
    -o "$output_file"
  )
  if [ -n "$extra_link_flag" ]; then
    command+=("$extra_link_flag")
  fi
  if [ -n "$selector" ]; then
    # AmigaPorts requires the runtime selector to appear exactly once and last.
    command+=("$selector")
  fi

  "${command[@]}" >"$log_file" 2>&1
}

inspect_binary() {
  local binary="$1"
  local output_dir="$2"
  local size_line

  if ! /usr/bin/file "$binary" | /usr/bin/grep -q 'AmigaOS loadseg()ble'; then
    printf 'Not an AmigaOS Hunk executable: %s\n' "$binary" >&2
    exit 1
  fi

  hunktool validate "$binary" >"$output_dir/hunk-validation.txt" || true
  if ! /usr/bin/grep -q 'TYPE_LOADSEG' "$output_dir/hunk-validation.txt"; then
    printf 'hunktool rejected: %s\n' "$binary" >&2
    exit 1
  fi

  "$MAGI80_SIZE" "$binary" >"$output_dir/size.txt"
  "$MAGI80_NM" --print-size --size-sort "$binary" >"$output_dir/symbols.txt"
  "$MAGI80_OBJDUMP" -dr "$binary" >"$output_dir/disassembly.txt"
  "$MAGI80_OBJDUMP" -f "$binary" >"$output_dir/format.txt"

  /bin/cp "$binary" "$output_dir/program.stripped"
  "$MAGI80_STRIP" -s "$output_dir/program.stripped"

  size_line="$(/usr/bin/awk 'NR == 2 { print $1, $2, $3 }' "$output_dir/size.txt")"
  read -r MAGI80_TEXT_BYTES MAGI80_DATA_BYTES MAGI80_BSS_BYTES <<<"$size_line"
  MAGI80_FILE_BYTES="$(/usr/bin/stat -f '%z' "$binary")"
  MAGI80_STRIPPED_BYTES="$(/usr/bin/stat -f '%z' "$output_dir/program.stripped")"
}

run_vamos() {
  local binary="$1"
  local output_dir="$2"
  local volume_dir="$output_dir/volume"

  /bin/mkdir -p "$volume_dir"
  vamos -q -C 20 -V "MAGI80:$volume_dir" "$binary" MAGI80: >"$output_dir/vamos.txt"
  /usr/bin/grep -qx 'result=pass' "$output_dir/vamos.txt"
  if /usr/bin/grep -q '^failure=' "$output_dir/vamos.txt"; then
    printf 'Runtime functional test failed under vamos: %s\n' "$binary" >&2
    exit 1
  fi
}

run_fs_uae() {
  local binary="$1"
  local expected_file="$2"
  local output_file="$3"
  local harness_log="${output_file%.txt}-harness.txt"

  if [ "$MAGI80_RUN_FS_UAE" = "0" ]; then
    printf 'skipped' >"$output_file"
    return
  fi

  "$MAGI80_PROJECT_ROOT/scripts/test-fs-uae-runtime.sh" \
    "$binary" "$expected_file" >"$harness_log"
  /bin/cp "$MAGI80_FS_UAE_MARKER" "$output_file"
}

/bin/rm -rf "$MAGI80_OUTPUT_DIR"
/bin/mkdir -p "$MAGI80_OUTPUT_DIR"

printf 'runtime\tselector\tfile_bytes\tstripped_bytes\ttext\tdata\tbss\tvamos\n' \
  >"$MAGI80_BASELINE_TSV"
printf 'runtime\tselector\tfull_api_link\ttested_binary\tfile_bytes\tstripped_bytes\ttext\tdata\tbss\tmalloc_zero\tvamos\tfs_uae\tnote\n' \
  >"$MAGI80_FUNCTIONAL_TSV"

for runtime in newlib libnix clib2; do
  selector=""
  case "$runtime" in
    newlib) selector_label='default' ;;
    libnix)
      selector='-mcrt=nix20'
      selector_label="$selector"
      ;;
    clib2)
      selector='-mcrt=clib2'
      selector_label="$selector"
      ;;
  esac

  runtime_dir="$MAGI80_OUTPUT_DIR/$runtime"
  baseline_dir="$runtime_dir/baseline"
  functional_dir="$runtime_dir/functional"
  /bin/mkdir -p "$baseline_dir" "$functional_dir"

  build_binary "$runtime" "$MAGI80_BOOTSTRAP_SOURCE" \
    "$baseline_dir/program" "$baseline_dir/program.map" \
    "$baseline_dir/link.log" '' '' "$selector"
  inspect_binary "$baseline_dir/program" "$baseline_dir"
  vamos -q -C 20 "$baseline_dir/program" >"$baseline_dir/vamos.txt"
  /usr/bin/diff -u "$MAGI80_PROJECT_ROOT/tests/smoke/hosted-bootstrap/expected.txt" \
    "$baseline_dir/vamos.txt"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\tpass\n' \
    "$runtime" "$selector_label" "$MAGI80_FILE_BYTES" "$MAGI80_STRIPPED_BYTES" \
    "$MAGI80_TEXT_BYTES" "$MAGI80_DATA_BYTES" "$MAGI80_BSS_BYTES" \
    >>"$MAGI80_BASELINE_TSV"

  full_api_link='pass'
  tested_binary='full'
  note='complete_required_api'
  extra_define=''
  extra_link_flag=''

  if ! build_binary "$runtime" "$MAGI80_MATRIX_SOURCE" \
      "$functional_dir/program" "$functional_dir/program.map" \
      "$functional_dir/full-link.log" '' '' "$selector"; then
    if [ "$runtime" != 'newlib' ] ||
       ! /usr/bin/grep -q "undefined reference to ._link'" "$functional_dir/full-link.log"; then
      printf 'Unexpected functional link failure for %s; see %s\n' \
        "$runtime" "$functional_dir/full-link.log" >&2
      exit 1
    fi

    full_api_link='fail_rename_undefined_link'
    tested_binary='compatibility_subset'
    note='requires_command_line_flag;rename_and_missing_errno_unavailable'
    extra_define='-DMAGI80_NEWLIB_COMPAT=1'
    extra_link_flag='-Wl,-u,___nocommandline'
    build_binary "$runtime" "$MAGI80_MATRIX_SOURCE" \
      "$functional_dir/program" "$functional_dir/program.map" \
      "$functional_dir/compat-link.log" "$extra_define" "$extra_link_flag" "$selector"
  fi

  inspect_binary "$functional_dir/program" "$functional_dir"
  run_vamos "$functional_dir/program" "$functional_dir"
  if [ "$runtime" = 'newlib' ] && [ "$MAGI80_RUN_FS_UAE" = "1" ]; then
    # The stock boot fixture deliberately provides no LIBS: contents.  Newlib's
    # printf path opens this floating-point library even with -msoft-float.
    printf 'mathieeedoubbas.library failed to load\n' \
      >"$functional_dir/fs-uae-expected.txt"
    run_fs_uae "$functional_dir/program" \
      "$functional_dir/fs-uae-expected.txt" "$functional_dir/fs-uae.txt"
  else
    run_fs_uae "$functional_dir/program" "$functional_dir/vamos.txt" \
      "$functional_dir/fs-uae.txt"
  fi

  malloc_zero="$(/usr/bin/awk -F= '$1 == "malloc_zero" { print $2 }' \
    "$functional_dir/vamos.txt")"
  if [ "$MAGI80_RUN_FS_UAE" = "1" ]; then
    if [ "$runtime" = 'newlib' ]; then
      fs_uae_status='fail_missing_mathieeedoubbas'
      note="$note;fs_uae_missing_mathieeedoubbas"
    elif [ "$full_api_link" = 'pass' ]; then
      fs_uae_status='pass'
    else
      fs_uae_status='compat_pass'
    fi
  else
    fs_uae_status='skipped'
  fi

  if [ "$full_api_link" = 'pass' ]; then
    vamos_status='pass'
  else
    vamos_status='compat_pass'
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$runtime" "$selector_label" "$full_api_link" "$tested_binary" \
    "$MAGI80_FILE_BYTES" "$MAGI80_STRIPPED_BYTES" "$MAGI80_TEXT_BYTES" \
    "$MAGI80_DATA_BYTES" "$MAGI80_BSS_BYTES" "$malloc_zero" \
    "$vamos_status" "$fs_uae_status" "$note" >>"$MAGI80_FUNCTIONAL_TSV"
done

{
  printf '# MAGI-80 C runtime comparison\n\n'
  printf 'Generated on 2026-09-02 with the pinned AmigaPorts toolchain.\n\n'
  printf '## Minimal hosted bootstrap\n\n'
  printf '| Runtime | Selector | Hunk bytes | Stripped | Text | Data | BSS | vamos |\n'
  printf '|---|---:|---:|---:|---:|---:|---:|---|\n'
  /usr/bin/awk -F '\t' 'NR > 1 { printf "| %s | `%s` | %s | %s | %s | %s | %s | %s |\n", $1, $2, $3, $4, $5, $6, $7, $8 }' \
    "$MAGI80_BASELINE_TSV"
  printf '\n## Allocation and filesystem matrix\n\n'
  printf '| Runtime | Full API link | Tested binary | Hunk bytes | Stripped | Text | Data | BSS | `malloc(0)` | vamos | FS-UAE | Note |\n'
  printf '|---|---|---|---:|---:|---:|---:|---:|---|---|---|---|\n'
  /usr/bin/awk -F '\t' 'NR > 1 { printf "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", $1, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13 }' \
    "$MAGI80_FUNCTIONAL_TSV"
  printf '\nThe newlib compatibility binary forces command-line startup and relaxes two failed requirements: ISO C `rename()` cannot link because the installed AmigaOS stubs do not define `_link`, and a missing `fopen()` does not set `errno`. On the stock FS-UAE boot fixture it also stops because its formatted-output path requires `mathieeedoubbas.library`, despite the soft-float target. It is measured for diagnosis but is not an eligible MAGI-80 runtime.\n'
} >"$MAGI80_OUTPUT_DIR/summary.md"

printf 'PASS  libnix and clib2 complete allocation/filesystem matrix\n'
printf 'PASS  newlib limitation reproduced and compatibility subset measured\n'
if [ "$MAGI80_RUN_FS_UAE" = "1" ]; then
  printf 'PASS  libnix and clib2 passed on the stock A1200 FS-UAE profile\n'
  printf 'PASS  newlib external math-library failure reproduced under FS-UAE\n'
fi
printf 'REPORT %s\n' "$MAGI80_OUTPUT_DIR/summary.md"
