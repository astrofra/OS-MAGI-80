#!/usr/bin/env bash

set -euo pipefail

MAGI80_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
MAGI80_VALIDATOR="$MAGI80_ROOT/scripts/validate-chipram-benchmark-report.sh"
MAGI80_FIXTURES="$MAGI80_ROOT/tests/host/chipram-benchmark-report"

"$MAGI80_VALIDATOR" "$MAGI80_FIXTURES/valid.txt" >/dev/null

if "$MAGI80_VALIDATOR" "$MAGI80_FIXTURES/invalid-stack.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL Chip-RAM report accepted an exhausted stack\n'
  exit 1
fi

if "$MAGI80_VALIDATOR" "$MAGI80_FIXTURES/invalid-checksum.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL Chip-RAM report accepted a checksum mismatch\n'
  exit 1
fi

printf 'PASS Chip-RAM report accepts the version 1 contract\n'
printf 'PASS Chip-RAM report rejects exhausted dedicated stacks\n'
printf 'PASS Chip-RAM report rejects checksum mismatches\n'
printf 'result=pass\n'
