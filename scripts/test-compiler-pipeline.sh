#!/bin/sh

set -eu

if [ "$#" -ne 8 ]; then
    printf 'Usage: %s miga80c miga68k-test source build-dir as objcopy report expected-report\n' \
        "$0" >&2
    exit 2
fi

compiler=$1
runner=$2
source=$3
pipeline_build_dir=$4
assembler=$5
objcopy=$6
report=$7
expected_report=$8
assembly="$pipeline_build_dir/generated.s"
object="$pipeline_build_dir/generated.o"
image="$pipeline_build_dir/generated.bin"

mkdir -p "$pipeline_build_dir" "$(dirname -- "$report")"
"$compiler" "$source" -S -o "$assembly"
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
    "$runner" --case "$image" "generated/$name" "$d0" "$d1" "$d2" \
        "$expected" >>"$report"
}

run_case zero 0 0 0
run_case positive 7 5 2
run_case negative_a -4 1 3
run_case negative_b 3 -7 -2
run_case wrap_u32 0x7fffffff 4 5
run_case min_i32 -2147483648 -1 -3
printf 'PASS  compiler typed-IR oracle matches Musashi (6 cases)\n' \
    >>"$report"

diff -u "$expected_report" "$report"
cat "$report"
