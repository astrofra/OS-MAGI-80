#!/usr/bin/env bash

set -euo pipefail

MAGI80_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
MAGI80_VALIDATOR="$MAGI80_ROOT/scripts/validate-exclusive-graphics-benchmark-report.sh"
MAGI80_VALID="$MAGI80_ROOT/tests/host/exclusive-graphics-benchmark-report/valid.txt"
MAGI80_TEMP=$(mktemp -d "${TMPDIR:-/tmp}/magi80-exclusive-report.XXXXXX")
trap 'rm -rf "$MAGI80_TEMP"' EXIT

"$MAGI80_VALIDATOR" "$MAGI80_VALID" >/dev/null

/usr/bin/sed '43s/sprite_dma=inactive/sprite_dma=active/' \
  "$MAGI80_VALID" >"$MAGI80_TEMP/invalid-dma.txt"
if "$MAGI80_VALIDATOR" "$MAGI80_TEMP/invalid-dma.txt" >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted a mismatched DMA profile\n'
  exit 1
fi

/usr/bin/sed '43s/actual_checksum=123456/actual_checksum=654321/' \
  "$MAGI80_VALID" >"$MAGI80_TEMP/invalid-checksum.txt"
if "$MAGI80_VALIDATOR" "$MAGI80_TEMP/invalid-checksum.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted a checksum mismatch\n'
  exit 1
fi

/usr/bin/sed '43s/maximum_ticks=1020/maximum_ticks=2000000/' \
  "$MAGI80_VALID" >"$MAGI80_TEMP/invalid-timer.txt"
if "$MAGI80_VALIDATOR" "$MAGI80_TEMP/invalid-timer.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted an implausible timer sample\n'
  exit 1
fi

printf 'PASS exclusive graphics report accepts the version 1 matrix contract\n'
printf 'PASS exclusive graphics report rejects a mismatched DMA profile\n'
printf 'PASS exclusive graphics report rejects checksum mismatches\n'
printf 'PASS exclusive graphics report rejects implausible timer samples\n'
printf 'result=pass\n'
