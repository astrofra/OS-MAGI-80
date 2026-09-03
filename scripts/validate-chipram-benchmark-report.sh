#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C
export LANG=C

MAGI80_REPORT="${1:-}"

if [ "$#" -ne 1 ] || [ ! -f "$MAGI80_REPORT" ]; then
  printf 'Usage: %s chipram-benchmark-report\n' "$0" >&2
  exit 1
fi

/usr/bin/awk '
function reject(message) {
  print "Invalid Chip-RAM benchmark report: " message > "/dev/stderr"
  failed = 1
}

function token(value) {
  return value ~ /^[a-z0-9_.-]+$/
}

function unsigned_decimal(value) {
  return value ~ /^[0-9]+$/
}

function positive_decimal(value) {
  return unsigned_decimal(value) && (value + 0) > 0
}

function parse_header(wanted, count) {
  count = split($0, pair, "=")
  if (count != 2 || pair[1] != wanted) {
    reject("expected header " wanted " on line " NR)
    return ""
  }
  return pair[2]
}

BEGIN {
  header_name[1] = "chipram_benchmark_format"
  header_name[2] = "environment"
  header_name[3] = "timing_authority"
  header_name[4] = "timing_scope"
  header_name[5] = "timing_source"
  header_name[6] = "eclock_hz"
  header_name[7] = "timer_overhead_ticks"
  header_name[8] = "display_dma"
  header_name[9] = "sprite_dma"
  header_name[10] = "audio_dma"
  header_name[11] = "interrupt_mode"
  header_name[12] = "buffer_bytes"
  header_name[13] = "batch_iterations"
  header_name[14] = "samples"
  header_name[15] = "dedicated_stack_bytes"
  header_name[16] = "stack_boundary_reserve_bytes"
  header_name[17] = "stack_guard_bytes"
  header_name[18] = "stack_high_water_bytes"
  header_name[19] = "initial_stack_bytes"
  header_name[20] = "interrupt_restore"
  header_name[21] = "case_count"

  required[1] = "case"
  required[2] = "operation"
  required[3] = "access_width"
  required[4] = "bytes_per_batch"
  required[5] = "minimum_chip_traffic_bytes"
  required[6] = "frame_budget_ticks"
  required[7] = "minimum_ticks"
  required[8] = "median_ticks"
  required[9] = "maximum_ticks"
  required[10] = "expected_checksum"
  required[11] = "actual_checksum"
  required[12] = "deadline_misses"
  required[13] = "kernel_return_checksum"
  required[14] = "result"
}

NR <= 21 {
  value = parse_header(header_name[NR])
  header_value[header_name[NR]] = value
  next
}

$0 == "result=pass" {
  if (footer_seen) reject("duplicate footer")
  footer_seen = 1
  footer_line = NR
  next
}

{
  if (footer_seen) {
    reject("content after footer on line " NR)
    next
  }
  if ($1 !~ /^case=/ || $NF != "result=pass") {
    reject("malformed case boundary on line " NR)
    next
  }
  for (old_key in field) delete field[old_key]
  for (number = 1; number <= NF; ++number) {
    count = split($number, pair, "=")
    key = pair[1]
    value = pair[2]
    if (count != 2 || key !~ /^[a-z][a-z0-9_]*$/ || !token(value)) {
      reject("malformed field on line " NR)
      continue
    }
    if (key in field) reject("duplicate field " key " on line " NR)
    field[key] = value
  }
  for (number = 1; number <= 14; ++number) {
    key = required[number]
    if (!(key in field)) reject("missing " key " on line " NR)
  }
  if (field["case"] in case_seen) reject("duplicate case " field["case"])
  case_seen[field["case"]] = 1

  numeric[1] = "bytes_per_batch"
  numeric[2] = "minimum_chip_traffic_bytes"
  numeric[3] = "frame_budget_ticks"
  numeric[4] = "minimum_ticks"
  numeric[5] = "median_ticks"
  numeric[6] = "maximum_ticks"
  numeric[7] = "expected_checksum"
  numeric[8] = "actual_checksum"
  numeric[9] = "deadline_misses"
  numeric[10] = "kernel_return_checksum"
  for (number = 1; number <= 10; ++number) {
    key = numeric[number]
    if (!unsigned_decimal(field[key])) reject("bad decimal " key " on line " NR)
  }
  if (!positive_decimal(field["bytes_per_batch"]) ||
      !positive_decimal(field["minimum_chip_traffic_bytes"]) ||
      !positive_decimal(field["frame_budget_ticks"])) {
    reject("zero required magnitude on line " NR)
  }
  if ((field["minimum_chip_traffic_bytes"] + 0) < (field["bytes_per_batch"] + 0)) {
    reject("traffic smaller than payload on line " NR)
  }
  if ((field["minimum_ticks"] + 0) > (field["median_ticks"] + 0) ||
      (field["median_ticks"] + 0) > (field["maximum_ticks"] + 0)) {
    reject("unordered timing values on line " NR)
  }
  if ((field["deadline_misses"] + 0) > (header_value["samples"] + 0)) {
    reject("deadline misses exceed samples on line " NR)
  }
  if ((field["expected_checksum"] + 0) != (field["actual_checksum"] + 0)) {
    reject("checksum mismatch on line " NR)
  }
  if (field["result"] != "pass") reject("case failed on line " NR)
  ++cases
}

END {
  if (header_value["chipram_benchmark_format"] != "1") {
    reject("unsupported format")
  }
  if (!token(header_value["environment"]) ||
      header_value["timing_authority"] != "protocol_only" ||
      header_value["timing_scope"] != "exclusive_kernel_batch" ||
      header_value["timing_source"] != "eclock" ||
      header_value["interrupt_mode"] != "custom_intena_masked" ||
      header_value["interrupt_restore"] != "pass") {
    reject("bad environment, authority, scope, or interrupt metadata")
  }
  if ((header_value["display_dma"] != "active" &&
       header_value["display_dma"] != "inactive") ||
      (header_value["sprite_dma"] != "active" &&
       header_value["sprite_dma"] != "inactive") ||
      (header_value["audio_dma"] != "active" &&
       header_value["audio_dma"] != "inactive")) {
    reject("bad DMA metadata")
  }
  positive_header[1] = "eclock_hz"
  positive_header[2] = "buffer_bytes"
  positive_header[3] = "batch_iterations"
  positive_header[4] = "samples"
  positive_header[5] = "dedicated_stack_bytes"
  positive_header[6] = "stack_boundary_reserve_bytes"
  positive_header[7] = "stack_guard_bytes"
  positive_header[8] = "stack_high_water_bytes"
  positive_header[9] = "initial_stack_bytes"
  positive_header[10] = "case_count"
  for (number = 1; number <= 10; ++number) {
    key = positive_header[number]
    if (!positive_decimal(header_value[key])) reject("bad header decimal " key)
  }
  if (!unsigned_decimal(header_value["timer_overhead_ticks"])) {
    reject("bad timer overhead")
  }
  if ((header_value["stack_high_water_bytes"] + 0) >= (header_value["dedicated_stack_bytes"] + 0)) {
    reject("dedicated stack exhausted")
  }
  if ((header_value["case_count"] + 0) != cases) {
    reject("case count mismatch")
  }
  if (!footer_seen || footer_line != NR) reject("missing final footer")
  if (failed) exit 1
}
' "$MAGI80_REPORT"

printf 'PASS Chip-RAM benchmark report schema and invariants\n'
