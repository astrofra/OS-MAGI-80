#!/usr/bin/env bash

set -euo pipefail

MIGA80_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
MIGA80_VALIDATOR="$MIGA80_ROOT/scripts/validate-exclusive-graphics-benchmark-report.sh"
MIGA80_VALID="$MIGA80_ROOT/tests/host/exclusive-graphics-benchmark-report/valid.txt"
MIGA80_VALID_V2="$MIGA80_ROOT/tests/host/exclusive-graphics-benchmark-report/valid-v2.txt"
MIGA80_TEMP=$(mktemp -d "${TMPDIR:-/tmp}/miga80-exclusive-report.XXXXXX")
trap 'rm -rf "$MIGA80_TEMP"' EXIT
MIGA80_VALID_V3="$MIGA80_TEMP/valid-v3.txt"
MIGA80_VALID_V3_FAST="$MIGA80_TEMP/valid-v3-fast.txt"

/usr/bin/awk '
NR == 1 {
  print "exclusive_graphics_benchmark_format=3"
  next
}
$0 == "case_count=7" {
  print "case_count=204"
  next
}
/^case=/ {
  if ($0 ~ / kind=raw /) {
    sub(/ result=pass$/,
        " source_memory=none destination_memory=chip lookup_memory=none source_offset=0 destination_offset=0 minimum_total_memory_traffic_bytes=32768 result=pass")
  } else if ($0 ~ / backend=pair_lut_m68k /) {
    sub(/ result=pass$/,
        " source_memory=chip destination_memory=chip lookup_memory=chip source_offset=0 destination_offset=0 minimum_total_memory_traffic_bytes=61440 result=pass")
  } else {
    sub(/ result=pass$/,
        " source_memory=chip destination_memory=chip lookup_memory=none source_offset=0 destination_offset=0 minimum_total_memory_traffic_bytes=30720 result=pass")
  }
  print
  if ($0 ~ /^case=blanked_raw_write_long4 /) {
    for (fixture = 1; fixture <= 115; ++fixture) {
      fixture_line = $0
      sub(/^case=[^ ]+/,
          sprintf("case=fixture_raw_%03d", fixture), fixture_line)
      print fixture_line
    }
  } else if ($0 ~ /^case=display_copper_sprite_audio_blitter_fair_c2p4_/) {
    for (fixture = 1; fixture <= 82; ++fixture) {
      fixture_line = $0
      sub(/^case=[^ ]+/,
          sprintf("case=fixture_c2p_%03d", fixture), fixture_line)
      print fixture_line
    }
  }
  next
}
{
  print
  if ($0 == "exclusive_timer_counter_bits=32") {
    print "raw_core_kernel_count=6"
    print "raw_extended_kernel_count=26"
    print "raw_extended_dma_profile_count=3"
    print "fast_matrix=not_present"
    print "fast_matrix_dma_profile_count=2"
    print "fast_case_count=0"
    print "stack_memory=chip"
    print "code_memory=chip"
    print "raw_result_count=120"
    print "c2p_result_count=84"
    print "baseline_case_count=204"
  }
}
' "$MIGA80_VALID_V2" >"$MIGA80_VALID_V3"

/usr/bin/awk '
$0 == "available_fast_bytes_before_setup=0" {
  print "available_fast_bytes_before_setup=2097152"
  next
}
$0 == "detected_stock_constraints=pass" {
  print "detected_stock_constraints=fail"
  next
}
$0 == "case_count=204" {
  print "case_count=260"
  next
}
$0 == "fast_matrix=not_present" {
  print "fast_matrix=active"
  next
}
$0 == "fast_case_count=0" {
  print "fast_case_count=56"
  next
}
$0 == "stack_memory=chip" {
  print "stack_memory=fast"
  next
}
$0 == "raw_result_count=120" {
  print "raw_result_count=152"
  next
}
$0 == "c2p_result_count=84" {
  print "c2p_result_count=108"
  next
}
$0 == "result=pass" {
  for (fixture = 1; fixture <= 32; ++fixture) {
    printf "case=fixture_fast_raw_%03d kind=raw dma_profile=blanked display_state=blanked display_dma=inactive copper_dma=inactive sprite_dma=inactive audio_dma=inactive blitter_dma=inactive display_plane_fetch_bytes_per_video_frame=0 minimum_controlled_sprite_fetch_bytes_per_video_frame=0 operation=read access_width=byte source_memory=fast destination_memory=none lookup_memory=none source_offset=0 destination_offset=0 blitter_launch_samples=0 blitter_busy_at_kernel_start_samples=0 blitter_busy_at_kernel_end_samples=0 blitter_copy_bytes=0 batch_iterations=4 samples=7 bytes_per_batch=32768 minimum_chip_traffic_bytes=0 minimum_total_memory_traffic_bytes=32768 frame_budget_ticks=28375 minimum_ticks=400 median_ticks=410 maximum_ticks=420 deadline_misses=0 expected_checksum=456789 actual_checksum=456789 kernel_return_checksum=987654 result=pass\n", fixture
  }
  for (fixture = 1; fixture <= 24; ++fixture) {
    printf "case=fixture_fast_c2p_%03d kind=c2p4 dma_profile=blanked display_state=blanked display_dma=inactive copper_dma=inactive sprite_dma=inactive audio_dma=inactive blitter_dma=inactive display_plane_fetch_bytes_per_video_frame=0 minimum_controlled_sprite_fetch_bytes_per_video_frame=0 source_layout=packed4 backend=pair_lut_m68k source_memory=fast destination_memory=chip lookup_memory=fast source_offset=0 destination_offset=0 blitter_launch_samples=0 blitter_busy_at_kernel_start_samples=0 blitter_busy_at_kernel_end_samples=0 blitter_copy_bytes=0 width=160 height=128 source_bytes=10240 plane_write_bytes=10240 lookup_traffic_bytes=40960 batch_iterations=1 samples=3 bytes_per_batch=10240 minimum_chip_traffic_bytes=10240 minimum_total_memory_traffic_bytes=61440 frame_budget_ticks=28375 minimum_ticks=1000 median_ticks=1010 maximum_ticks=1020 deadline_misses=0 expected_checksum=567890 actual_checksum=567890 destination=pf1 result=pass\n", fixture
  }
}
{ print }
' "$MIGA80_VALID_V3" >"$MIGA80_VALID_V3_FAST"

"$MIGA80_VALIDATOR" "$MIGA80_VALID" >/dev/null
"$MIGA80_VALIDATOR" "$MIGA80_VALID_V2" >/dev/null
"$MIGA80_VALIDATOR" "$MIGA80_VALID_V3" >/dev/null
"$MIGA80_VALIDATOR" "$MIGA80_VALID_V3_FAST" >/dev/null

/usr/bin/sed '43s/sprite_dma=inactive/sprite_dma=active/' \
  "$MIGA80_VALID" >"$MIGA80_TEMP/invalid-dma.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-dma.txt" >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted a mismatched DMA profile\n'
  exit 1
fi

/usr/bin/sed '43s/actual_checksum=123456/actual_checksum=654321/' \
  "$MIGA80_VALID" >"$MIGA80_TEMP/invalid-checksum.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-checksum.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted a checksum mismatch\n'
  exit 1
fi

/usr/bin/sed '43s/maximum_ticks=1020/maximum_ticks=2000000/' \
  "$MIGA80_VALID" >"$MIGA80_TEMP/invalid-timer.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-timer.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted an implausible timer sample\n'
  exit 1
fi

/usr/bin/sed \
  's/blitter_busy_at_kernel_end_samples=3/blitter_busy_at_kernel_end_samples=2/' \
  "$MIGA80_VALID_V2" >"$MIGA80_TEMP/invalid-blitter-span.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-blitter-span.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted an incomplete blitter load\n'
  exit 1
fi

/usr/bin/sed \
  's/exclusive_timer_resource=ciaa/exclusive_timer_resource=timer.device/' \
  "$MIGA80_VALID_V2" >"$MIGA80_TEMP/invalid-exclusive-timer.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-exclusive-timer.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted an unreserved timer source\n'
  exit 1
fi

/usr/bin/awk '
!changed && /^case=/ && /minimum_total_memory_traffic_bytes=32768/ {
  sub(/minimum_total_memory_traffic_bytes=32768/,
      "minimum_total_memory_traffic_bytes=65536")
  changed = 1
}
{ print }
' "$MIGA80_VALID_V3" >"$MIGA80_TEMP/invalid-total-traffic.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-total-traffic.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted bad total traffic\n'
  exit 1
fi

/usr/bin/sed 's/fast_matrix=not_present/fast_matrix=active/' \
  "$MIGA80_VALID_V3" >"$MIGA80_TEMP/invalid-fast-matrix.txt"
if "$MIGA80_VALIDATOR" "$MIGA80_TEMP/invalid-fast-matrix.txt" \
    >/dev/null 2>&1; then
  printf 'FAIL exclusive graphics report accepted inconsistent Fast metadata\n'
  exit 1
fi

printf 'PASS exclusive graphics report accepts the version 1 matrix contract\n'
printf 'PASS exclusive graphics report accepts the version 2 contention contract\n'
printf 'PASS exclusive graphics report accepts the version 3 stock matrix contract\n'
printf 'PASS exclusive graphics report accepts the version 3 Fast-assisted contract\n'
printf 'PASS exclusive graphics report rejects a mismatched DMA profile\n'
printf 'PASS exclusive graphics report rejects checksum mismatches\n'
printf 'PASS exclusive graphics report rejects implausible timer samples\n'
printf 'PASS exclusive graphics report rejects incomplete blitter overlap\n'
printf 'PASS exclusive graphics report rejects an unreserved timer source\n'
printf 'PASS exclusive graphics report rejects bad total traffic accounting\n'
printf 'PASS exclusive graphics report rejects inconsistent Fast metadata\n'
printf 'result=pass\n'
