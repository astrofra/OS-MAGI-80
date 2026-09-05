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
narrow_build_dir=$4
assembler=$5
objcopy=$6
report=$7
expected_report=$8
assembly_o0="$narrow_build_dir/generated-o0.s"
object_o0="$narrow_build_dir/generated-o0.o"
image_o0="$narrow_build_dir/generated-o0.bin"
assembly_o1="$narrow_build_dir/generated-o1.s"
object_o1="$narrow_build_dir/generated-o1.o"
image_o1="$narrow_build_dir/generated-o1.bin"
assembly_default="$narrow_build_dir/generated.s"
evaluation_error="$narrow_build_dir/evaluation-error.txt"

mkdir -p "$narrow_build_dir" "$(dirname -- "$report")"
"$compiler" "$source" -O0 -S -o "$assembly_o0"
"$assembler" -m68020 "$assembly_o0" -o "$object_o0"
"$objcopy" -O binary -j .text "$object_o0" "$image_o0"
"$compiler" "$source" -O1 -S -o "$assembly_o1"
"$compiler" "$source" -S -o "$assembly_default"
cmp "$assembly_o1" "$assembly_default"
"$assembler" -m68020 "$assembly_o1" -o "$object_o1"
"$objcopy" -O binary -j .text "$object_o1" "$image_o1"

: >"$report"
run_value_case()
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

run_fault_case()
{
    image=$1
    level=$2

    if "$compiler" "$source" --eval 0 254 0 \
            >/dev/null 2>"$evaluation_error"; then
        printf 'typed-IR oracle accepted narrow division by zero for %s\n' \
            "$level" >&2
        exit 1
    fi
    if ! grep -Fqx "$source:6:12: error: division by zero" \
            "$evaluation_error"; then
        printf 'typed-IR oracle reported the wrong narrow division fault for %s\n' \
            "$level" >&2
        sed -n '1,4p' "$evaluation_error" >&2
        exit 1
    fi
    "$runner" --fault-case "$image" "$level/div_zero" 0 254 0 \
        1 6 12 >>"$report"
}

run_suite()
{
    image=$1
    level=$2

    run_value_case "$image" "$level" zero 0 0 0
    run_value_case "$image" "$level" signed_min -128 255 -32768
    run_value_case "$image" "$level" signed_max 127 250 6
    run_value_case "$image" "$level" signed_division 0 0 6
    run_value_case "$image" "$level" signed_rounding 0 0 4
    run_value_case "$image" "$level" unsigned_boundary -2 200 -7
    run_fault_case "$image" "$level"
}

run_suite "$image_o0" generated-o0
run_suite "$image_o1" generated-o1
o0_size=$(wc -c <"$image_o0" | tr -d '[:space:]')
o1_size=$(wc -c <"$image_o1" | tr -d '[:space:]')
if [ "$o1_size" -ge "$o0_size" ]; then
    printf 'O1 narrow-integer code-size regression: O0=%s bytes, O1=%s bytes\n' \
        "$o0_size" "$o1_size" >&2
    exit 1
fi
printf 'PASS  compiler narrow integers match typed-IR oracle (12 values; 2 controlled faults)\n' \
    >>"$report"
printf 'PASS  compiler O1 narrow-integer code size reduced from %s to %s bytes\n' \
    "$o0_size" "$o1_size" >>"$report"

diff -u "$expected_report" "$report"
sed -n '1,$p' "$report"
