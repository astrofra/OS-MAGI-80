#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C
export LANG=C

MAGI80_REPORT="${1:-}"

if [ "$#" -ne 1 ] || [ ! -f "$MAGI80_REPORT" ]; then
  printf 'Usage: %s graphics-benchmark-report\n' "$0" >&2
  exit 1
fi

/usr/bin/awk '
function reject(message) {
  print "Invalid graphics benchmark report: " message > "/dev/stderr"
  failed = 1
}

function safe_token(value) {
  return value ~ /^[a-z0-9_.-]+$/
}

function unsigned_decimal(value) {
  return value ~ /^[0-9]+$/
}

function positive_decimal(value) {
  return unsigned_decimal(value) && (value + 0) > 0
}

function parse_header(wanted, value, pair_count) {
  pair_count = split($0, pair, "=")
  if (pair_count != 2 || pair[1] != wanted) {
    reject("expected header " wanted " on line " NR)
    return ""
  }
  return pair[2]
}

BEGIN {
  header_name[1] = "graphics_benchmark_format"
  header_name[2] = "benchmark"
  header_name[3] = "environment"
  header_name[4] = "timing_authority"
  header_name[5] = "timing_source"
  header_name[6] = "eclock_hz"
  header_name[7] = "timer_overhead_ticks"
  header_name[8] = "canonical_format"
  header_name[9] = "checksum_algorithm"
  header_name[10] = "display_dma"
  header_name[11] = "sprite_dma"
  header_name[12] = "audio_dma"
  header_name[13] = "case_count"
  for (header_number = 1; header_number <= 13; ++header_number) {
    header_reserved[header_name[header_number]] = 1
  }

  required[1] = "case"
  required[2] = "backend"
  required[3] = "workload"
  required[4] = "width"
  required[5] = "height"
  required[6] = "samples"
  required[7] = "memory_bytes"
  required[8] = "dirty_bytes"
  required[9] = "dirty_regions"
  required[10] = "objects"
  required[11] = "fallback_objects"
  required[12] = "frame_budget_ticks"
  required[13] = "source_median_ticks"
  required[14] = "draw_median_ticks"
  required[15] = "cpu_conversion_median_ticks"
  required[16] = "blitter_median_ticks"
  required[17] = "blitter_wait_median_ticks"
  required[18] = "publication_median_ticks"
  required[19] = "total_min_ticks"
  required[20] = "total_median_ticks"
  required[21] = "total_max_ticks"
  required[22] = "oracle_checksum"
  required[23] = "canonical_checksum"
  required[24] = "deadline_misses"
  required[25] = "result"
}

NR <= 13 {
  value = parse_header(header_name[NR])
  header_value[header_name[NR]] = value
  next
}

$0 == "result=pass" {
  if (footer_seen) reject("duplicate result footer")
  footer_seen = 1
  footer_line = NR
  next
}

{
  if (footer_seen) {
    reject("content after result footer on line " NR)
    next
  }
  if ($1 !~ /^case=/) {
    reject("expected case record on line " NR)
    next
  }
  if ($NF != "result=pass") {
    reject("case result must be the final field on line " NR)
  }

  for (old_key in field_value) delete field_value[old_key]
  for (field_number = 1; field_number <= NF; ++field_number) {
    pair_count = split($field_number, pair, "=")
    key = pair[1]
    value = pair[2]
    if (pair_count != 2 || key !~ /^[a-z][a-z0-9_]*$/ ||
        !safe_token(value)) {
      reject("malformed field " field_number " on line " NR)
      continue
    }
    if (key in field_value) {
      reject("duplicate field " key " on line " NR)
    }
    if (key in header_reserved) {
      reject("header field repeated in case on line " NR)
    }
    field_value[key] = value
  }

  for (required_number = 1; required_number <= 25; ++required_number) {
    key = required[required_number]
    if (!(key in field_value)) {
      reject("missing " key " on line " NR)
    }
  }
  if ((field_value["case"] in case_seen)) {
    reject("duplicate case " field_value["case"])
  }
  case_seen[field_value["case"]] = 1

  numeric[1] = "width"
  numeric[2] = "height"
  numeric[3] = "samples"
  numeric[4] = "memory_bytes"
  numeric[5] = "dirty_bytes"
  numeric[6] = "dirty_regions"
  numeric[7] = "objects"
  numeric[8] = "fallback_objects"
  numeric[9] = "frame_budget_ticks"
  numeric[10] = "source_median_ticks"
  numeric[11] = "draw_median_ticks"
  numeric[12] = "cpu_conversion_median_ticks"
  numeric[13] = "blitter_median_ticks"
  numeric[14] = "blitter_wait_median_ticks"
  numeric[15] = "publication_median_ticks"
  numeric[16] = "total_min_ticks"
  numeric[17] = "total_median_ticks"
  numeric[18] = "total_max_ticks"
  numeric[19] = "oracle_checksum"
  numeric[20] = "canonical_checksum"
  numeric[21] = "deadline_misses"
  for (numeric_number = 1; numeric_number <= 21; ++numeric_number) {
    key = numeric[numeric_number]
    if (!unsigned_decimal(field_value[key])) {
      reject("bad decimal " key " on line " NR)
    }
  }
  if (!positive_decimal(field_value["width"]) ||
      !positive_decimal(field_value["height"]) ||
      !positive_decimal(field_value["samples"]) ||
      !positive_decimal(field_value["memory_bytes"]) ||
      !positive_decimal(field_value["frame_budget_ticks"])) {
    reject("zero-valued required magnitude on line " NR)
  }
  min_ticks = field_value["total_min_ticks"] + 0
  median_ticks = field_value["total_median_ticks"] + 0
  max_ticks = field_value["total_max_ticks"] + 0
  if (min_ticks > median_ticks || median_ticks > max_ticks) {
    reject("unordered timing values on line " NR)
  }
  if ((field_value["canonical_checksum"] + 0) > 4294967295) {
    reject("checksum outside unsigned 32-bit range on line " NR)
  }
  oracle_checksum = field_value["oracle_checksum"] + 0
  canonical_checksum = field_value["canonical_checksum"] + 0
  if (oracle_checksum > 4294967295 ||
      oracle_checksum != canonical_checksum) {
    reject("output does not match oracle checksum on line " NR)
  }
  misses = field_value["deadline_misses"] + 0
  sample_count = field_value["samples"] + 0
  if (misses > sample_count) {
    reject("deadline misses exceed samples on line " NR)
  }
  fallback_count = field_value["fallback_objects"] + 0
  object_count = field_value["objects"] + 0
  if (fallback_count > object_count) {
    reject("fallback objects exceed object count on line " NR)
  }
  if (field_value["result"] != "pass") {
    reject("case did not pass on line " NR)
  }
  ++cases
}

END {
  if (header_value["graphics_benchmark_format"] != "1") {
    reject("unsupported format version")
  }
  if (!safe_token(header_value["benchmark"]) ||
      !safe_token(header_value["environment"])) {
    reject("bad benchmark or environment token")
  }
  if (header_value["timing_authority"] != "protocol_only" &&
      header_value["timing_authority"] != "real_hardware") {
    reject("bad timing authority")
  }
  if (header_value["timing_source"] != "eclock") {
    reject("unsupported timing source")
  }
  if (!positive_decimal(header_value["eclock_hz"]) ||
      !unsigned_decimal(header_value["timer_overhead_ticks"])) {
    reject("bad E-Clock metadata")
  }
  if (header_value["canonical_format"] != "palette_identity_u8" ||
      header_value["checksum_algorithm"] != "fnv1a32") {
    reject("unsupported correctness encoding")
  }
  if ((header_value["display_dma"] != "active" &&
       header_value["display_dma"] != "inactive") ||
      (header_value["sprite_dma"] != "active" &&
       header_value["sprite_dma"] != "inactive") ||
      (header_value["audio_dma"] != "active" &&
       header_value["audio_dma"] != "inactive")) {
    reject("bad DMA state")
  }
  if (!positive_decimal(header_value["case_count"]) ||
      (header_value["case_count"] + 0) != cases) {
    reject("case count mismatch")
  }
  if (!footer_seen || footer_line != NR) {
    reject("missing final result footer")
  }
  if (failed) exit 1
}
' "$MAGI80_REPORT"

printf 'PASS graphics benchmark report schema and invariants\n'
