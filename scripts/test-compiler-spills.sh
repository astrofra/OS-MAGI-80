#!/bin/sh

set -eu

if [ "$#" -ne 9 ]; then
    printf 'Usage: %s generator miga80c runner source build-dir as objcopy report expected-report\n' \
        "$0" >&2
    exit 2
fi

generator=$1
compiler=$2
runner=$3
source=$4
spill_build_dir=$5
assembler=$6
objcopy=$7
report=$8
expected_report=$9
assembly="$spill_build_dir/generated-o1-spill.s"
object="$spill_build_dir/generated-o1-spill.o"
image="$spill_build_dir/generated-o1-spill.bin"

mkdir -p "$spill_build_dir" "$(dirname -- "$report")"
"$generator" --emit-spill-fixture >"$assembly"
"$assembler" -m68020 "$assembly" -o "$object"
"$objcopy" -O binary -j .text "$object" "$image"

: >"$report"
run_case()
{
    name=$1
    d0=$2
    d1=$3
    d2=$4
    expected=$("$compiler" "$source" --eval "$d0" "$d1" "$d2")
    "$runner" --case "$image" "generated-o1-spill/$name" \
        "$d0" "$d1" "$d2" "$expected" >>"$report"
}

run_case zero 0 0 0
run_case positive 7 5 2
run_case negative_a -4 1 3
run_case negative_b 3 -7 -2
run_case wrap_u32 0x7fffffff 4 5
run_case min_i32 -2147483648 -1 -3
printf 'PASS  compiler O1 spill frame matches typed-IR oracle (6 cases)\n' \
    >>"$report"

diff -u "$expected_report" "$report"
cat "$report"
