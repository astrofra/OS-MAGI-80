#!/bin/sh

set -eu

if [ "$#" -ne 9 ]; then
    printf 'Usage: %s miga80c miga68k-test source build-dir as objcopy objdump report expected-report\n' \
        "$0" >&2
    exit 2
fi

compiler=$1
runner=$2
source=$3
immutable_build_dir=$4
assembler=$5
objcopy=$6
objdump=$7
report=$8
expected_report=$9
assembly_o0="$immutable_build_dir/generated-o0.s"
object_o0="$immutable_build_dir/generated-o0.o"
image_o0="$immutable_build_dir/generated-o0.bin"
assembly_o1="$immutable_build_dir/generated-o1.s"
object_o1="$immutable_build_dir/generated-o1.o"
image_o1="$immutable_build_dir/generated-o1.bin"
assembly_default="$immutable_build_dir/generated.s"

mkdir -p "$immutable_build_dir" "$(dirname -- "$report")"
"$compiler" "$source" -O0 -S -o "$assembly_o0"
"$assembler" -m68020 "$assembly_o0" -o "$object_o0"
"$objcopy" -O binary -j .text "$object_o0" "$image_o0"
"$compiler" "$source" -O1 -S -o "$assembly_o1"
"$compiler" "$source" -S -o "$assembly_default"
cmp "$assembly_o1" "$assembly_default"
"$assembler" -m68020 "$assembly_o1" -o "$object_o1"
"$objcopy" -O binary -j .text "$object_o1" "$image_o1"

for object in "$object_o0" "$object_o1"; do
    if "$objdump" -r "$object" | grep -Fq 'RELOCATION RECORDS'; then
        printf 'immutable pool left a relocation in %s\n' "$object" >&2
        exit 1
    fi
done

for assembly in "$assembly_o0" "$assembly_o1"; do
    test "$(grep -Fc '.L_immutable_values_string_0:' "$assembly")" -eq 1
    test "$(grep -Fc '.L_immutable_values_string_2:' "$assembly")" -eq 1
    test "$(grep -Fc '.L_immutable_values_string_1:' "$assembly")" -eq 0
    grep -Fq '.long   5' "$assembly"
    grep -Fq '0x73,0x61,0x6d,0x65,0x0a' "$assembly"
done

: >"$report"
run_case()
{
    image=$1
    level=$2
    name=$3
    flag=$4
    expected=$("$compiler" "$source" --eval "$flag")

    "$runner" --case "$image" "$level/$name" "$flag" 0 0 \
        "$expected" >>"$report"
}

run_case "$image_o0" generated-o0 false 0
run_case "$image_o0" generated-o0 true 1
run_case "$image_o1" generated-o1 false 0
run_case "$image_o1" generated-o1 true 1

o0_size=$(wc -c <"$image_o0" | tr -d '[:space:]')
o1_size=$(wc -c <"$image_o1" | tr -d '[:space:]')
if [ "$o1_size" -ge "$o0_size" ]; then
    printf 'O1 immutable-value code-size regression: O0=%s bytes, O1=%s bytes\n' \
        "$o0_size" "$o1_size" >&2
    exit 1
fi
printf 'PASS  compiler immutable strings/symbols match typed-IR oracle (4 cases)\n' \
    >>"$report"
printf 'PASS  compiler O1 immutable-value code size reduced from %s to %s bytes\n' \
    "$o0_size" "$o1_size" >>"$report"

diff -u "$expected_report" "$report"
cat "$report"
