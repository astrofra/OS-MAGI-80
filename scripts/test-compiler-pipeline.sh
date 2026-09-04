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
assembly_o0="$pipeline_build_dir/generated-o0.s"
object_o0="$pipeline_build_dir/generated-o0.o"
image_o0="$pipeline_build_dir/generated-o0.bin"
assembly_o1="$pipeline_build_dir/generated-o1.s"
object_o1="$pipeline_build_dir/generated-o1.o"
image_o1="$pipeline_build_dir/generated-o1.bin"
assembly_default="$pipeline_build_dir/generated.s"

mkdir -p "$pipeline_build_dir" "$(dirname -- "$report")"
"$compiler" "$source" -O0 -S -o "$assembly_o0"
"$assembler" -m68020 "$assembly_o0" -o "$object_o0"
"$objcopy" -O binary -j .text "$object_o0" "$image_o0"
"$compiler" "$source" -O1 -S -o "$assembly_o1"
"$compiler" "$source" -S -o "$assembly_default"
cmp "$assembly_o1" "$assembly_default"
"$assembler" -m68020 "$assembly_o1" -o "$object_o1"
"$objcopy" -O binary -j .text "$object_o1" "$image_o1"

: >"$report"
run_case()
{
    image=$1
    level=$2
    name=$3
    d0=$4
    d1=$5
    d2=$6
    expected=$("$compiler" "$source" --eval "$d0" "$d1" "$d2")
    "$runner" --case "$image" "$level/$name" "$d0" "$d1" "$d2" \
        "$expected" >>"$report"
}

run_suite()
{
    image=$1
    level=$2

    run_case "$image" "$level" zero 0 0 0
    run_case "$image" "$level" positive 7 5 2
    run_case "$image" "$level" negative_a -4 1 3
    run_case "$image" "$level" negative_b 3 -7 -2
    run_case "$image" "$level" wrap_u32 0x7fffffff 4 5
    run_case "$image" "$level" min_i32 -2147483648 -1 -3
}

run_suite "$image_o0" generated-o0
run_suite "$image_o1" generated-o1
o0_size=$(wc -c <"$image_o0" | tr -d '[:space:]')
o1_size=$(wc -c <"$image_o1" | tr -d '[:space:]')
if [ "$o1_size" -ge "$o0_size" ]; then
    printf 'O1 code-size regression: O0=%s bytes, O1=%s bytes\n' \
        "$o0_size" "$o1_size" >&2
    exit 1
fi
printf 'PASS  compiler O0/O1 match typed-IR oracle (12 cases)\n' \
    >>"$report"
printf 'PASS  compiler O1 code size reduced from %s to %s bytes\n' \
    "$o0_size" "$o1_size" >>"$report"

diff -u "$expected_report" "$report"
cat "$report"
