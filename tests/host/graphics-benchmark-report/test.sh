#!/usr/bin/env bash

set -euo pipefail

MIGA80_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
MIGA80_VALIDATOR="$MIGA80_ROOT/scripts/validate-graphics-benchmark-report.sh"
MIGA80_FIXTURES="$MIGA80_ROOT/tests/host/graphics-benchmark-report"

"$MIGA80_VALIDATOR" "$MIGA80_FIXTURES/valid.txt" >/dev/null

if "$MIGA80_VALIDATOR" "$MIGA80_FIXTURES/invalid-count.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL graphics report accepted a wrong case count\n'
  exit 1
fi

if "$MIGA80_VALIDATOR" "$MIGA80_FIXTURES/invalid-timing.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL graphics report accepted unordered timings\n'
  exit 1
fi

if "$MIGA80_VALIDATOR" "$MIGA80_FIXTURES/invalid-checksum.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL graphics report accepted an oracle mismatch\n'
  exit 1
fi

printf 'PASS graphics report accepts the version 1 contract\n'
printf 'PASS graphics report rejects wrong case counts\n'
printf 'PASS graphics report rejects unordered timings\n'
printf 'PASS graphics report rejects oracle mismatches\n'
printf 'result=pass\n'
