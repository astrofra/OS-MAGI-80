#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C
export LANG=C

MAGI80_REPORT="${1:-}"

if [ "$#" -ne 1 ] || [ ! -f "$MAGI80_REPORT" ]; then
  printf 'Usage: %s exclusive-graphics-benchmark-report\n' "$0" >&2
  exit 1
fi

/usr/bin/awk '
function reject(message) {
  print "Invalid exclusive graphics benchmark report: " message > "/dev/stderr"
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

function require_field(name) {
  if (!(name in field)) reject("missing " name " on line " NR)
}

function expect_dma(profile, display, copper, sprite, audio, blitter) {
  profile_known[profile] = 1
  profile_display[profile] = display
  profile_copper[profile] = copper
  profile_sprite[profile] = sprite
  profile_audio[profile] = audio
  profile_blitter[profile] = blitter
}

BEGIN {
  header_count = 32
  header_name[1] = "exclusive_graphics_benchmark_format"
  header_name[2] = "benchmark"
  header_name[3] = "environment"
  header_name[4] = "timing_authority"
  header_name[5] = "timing_scope"
  header_name[6] = "timing_source"
  header_name[7] = "eclock_hz"
  header_name[8] = "timer_overhead_ticks"
  header_name[9] = "timer_discarded_samples"
  header_name[10] = "interrupt_mode"
  header_name[11] = "source_memory"
  header_name[12] = "destination_memory"
  header_name[13] = "display_mode"
  header_name[14] = "video_hz"
  header_name[15] = "screen_owner"
  header_name[16] = "publication"
  header_name[17] = "sprite_load"
  header_name[18] = "audio_load"
  header_name[19] = "blitter_load"
  header_name[20] = "raster_start_line"
  header_name[21] = "dma_profile_count"
  header_name[22] = "audio_channels"
  header_name[23] = "blitter_copy_bytes"
  header_name[24] = "dedicated_stack_bytes"
  header_name[25] = "stack_boundary_reserve_bytes"
  header_name[26] = "stack_guard_bytes"
  header_name[27] = "stack_high_water_bytes"
  header_name[28] = "initial_stack_bytes"
  header_name[29] = "interrupt_restore"
  header_name[30] = "dma_restore"
  header_name[31] = "checksum_algorithm"
  header_name[32] = "case_count"

  common[1] = "case"
  common[2] = "kind"
  common[3] = "dma_profile"
  common[4] = "display_state"
  common[5] = "display_dma"
  common[6] = "copper_dma"
  common[7] = "sprite_dma"
  common[8] = "audio_dma"
  common[9] = "blitter_dma"
  common[10] = "display_plane_fetch_bytes_per_video_frame"
  common[11] = "batch_iterations"
  common[12] = "samples"
  common[13] = "bytes_per_batch"
  common[14] = "minimum_chip_traffic_bytes"
  common[15] = "frame_budget_ticks"
  common[16] = "minimum_ticks"
  common[17] = "median_ticks"
  common[18] = "maximum_ticks"
  common[19] = "deadline_misses"
  common[20] = "expected_checksum"
  common[21] = "actual_checksum"
  common[22] = "result"

  numeric[1] = "batch_iterations"
  numeric[2] = "samples"
  numeric[3] = "bytes_per_batch"
  numeric[4] = "minimum_chip_traffic_bytes"
  numeric[5] = "frame_budget_ticks"
  numeric[6] = "minimum_ticks"
  numeric[7] = "median_ticks"
  numeric[8] = "maximum_ticks"
  numeric[9] = "deadline_misses"
  numeric[10] = "expected_checksum"
  numeric[11] = "actual_checksum"
  numeric[12] = "display_plane_fetch_bytes_per_video_frame"

  expect_dma("blanked", "inactive", "inactive", "inactive", "inactive", "inactive")
  expect_dma("display", "active", "inactive", "inactive", "inactive", "inactive")
  expect_dma("display_copper", "active", "active", "inactive", "inactive", "inactive")
  expect_dma("display_copper_sprite", "active", "active", "active", "inactive", "inactive")
  expect_dma("display_copper_sprite_audio", "active", "active", "active", "active", "inactive")
  expect_dma("display_copper_sprite_audio_blitter_fair", "active", "active", "active", "active", "busy_fair")
  expect_dma("display_copper_sprite_audio_blitter_hog", "active", "active", "active", "active", "busy_hog")
}

NR <= header_count {
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
  for (number = 1; number <= 22; ++number) require_field(common[number])
  if (field["case"] in case_seen) reject("duplicate case " field["case"])
  case_seen[field["case"]] = 1

  for (number = 1; number <= 12; ++number) {
    key = numeric[number]
    if (!unsigned_decimal(field[key])) reject("bad decimal " key " on line " NR)
  }
  if (!positive_decimal(field["batch_iterations"]) ||
      !positive_decimal(field["samples"]) ||
      !positive_decimal(field["bytes_per_batch"]) ||
      !positive_decimal(field["minimum_chip_traffic_bytes"]) ||
      !positive_decimal(field["frame_budget_ticks"])) {
    reject("zero required magnitude on line " NR)
  }
  if ((field["minimum_ticks"] + 0) > (field["median_ticks"] + 0) ||
      (field["median_ticks"] + 0) > (field["maximum_ticks"] + 0)) {
    reject("unordered timing values on line " NR)
  }
  maximum_plausible_ticks = (header_value["eclock_hz"] + 0) * 2
  if ((field["maximum_ticks"] + 0) > maximum_plausible_ticks) {
    reject("implausible timing sample on line " NR)
  }
  if ((field["deadline_misses"] + 0) > (field["samples"] + 0)) {
    reject("deadline misses exceed samples on line " NR)
  }
  expected_checksum = field["expected_checksum"] + 0
  actual_checksum = field["actual_checksum"] + 0
  if (expected_checksum != actual_checksum) {
    reject("checksum mismatch on line " NR)
  }
  if (field["result"] != "pass") reject("case failed on line " NR)

  profile = field["dma_profile"]
  if (!(profile in profile_known)) {
    reject("unknown DMA profile on line " NR)
  } else {
    profile_seen[profile] = 1
    expected_display_state = profile_display[profile] == "active" ? "active" : "blanked"
    expected_display_fetch = profile_display[profile] == "active" ? 65536 : 0
    actual_display_fetch = field["display_plane_fetch_bytes_per_video_frame"] + 0
    if (field["display_state"] != expected_display_state ||
        actual_display_fetch != expected_display_fetch ||
        field["display_dma"] != profile_display[profile] ||
        field["copper_dma"] != profile_copper[profile] ||
        field["sprite_dma"] != profile_sprite[profile] ||
        field["audio_dma"] != profile_audio[profile] ||
        field["blitter_dma"] != profile_blitter[profile]) {
      reject("DMA state does not match profile " profile " on line " NR)
    }
  }

  if (field["kind"] == "raw") {
    require_field("operation")
    require_field("access_width")
    require_field("kernel_return_checksum")
    if (!unsigned_decimal(field["kernel_return_checksum"])) {
      reject("bad raw kernel checksum on line " NR)
    }
    if (field["batch_iterations"] != "4" || field["samples"] != "7") {
      reject("bad raw sampling contract on line " NR)
    }
    minimum_traffic = field["minimum_chip_traffic_bytes"] + 0
    batch_bytes = field["bytes_per_batch"] + 0
    if (minimum_traffic < batch_bytes) {
      reject("raw traffic smaller than payload on line " NR)
    }
  } else if (field["kind"] == "c2p4") {
    require_field("source_layout")
    require_field("backend")
    require_field("width")
    require_field("height")
    require_field("source_bytes")
    require_field("plane_write_bytes")
    require_field("lookup_traffic_bytes")
    require_field("destination")
    c2p_numeric[1] = "width"
    c2p_numeric[2] = "height"
    c2p_numeric[3] = "source_bytes"
    c2p_numeric[4] = "plane_write_bytes"
    c2p_numeric[5] = "lookup_traffic_bytes"
    for (number = 1; number <= 5; ++number) {
      key = c2p_numeric[number]
      if (!positive_decimal(field[key]) && key != "lookup_traffic_bytes") {
        reject("bad C2P decimal " key " on line " NR)
      } else if (key == "lookup_traffic_bytes" &&
                 !unsigned_decimal(field[key])) {
        reject("bad C2P decimal " key " on line " NR)
      }
    }
    pixels = (field["width"] + 0) * (field["height"] + 0)
    expected_source = field["source_layout"] == "packed4" ? pixels / 2 : pixels
    expected_lookup = field["backend"] == "pair_lut_m68k" ? pixels * 2 : 0
    expected_traffic = expected_source + (pixels / 2) + expected_lookup
    if ((field["source_layout"] != "packed4" &&
         field["source_layout"] != "byte4") ||
        (field["backend"] != "pair_lut_m68k" &&
         field["backend"] != "mask32_m68k") ||
        field["destination"] != "pf1") {
      reject("bad C2P mode on line " NR)
    }
    if ((field["source_bytes"] + 0) != expected_source ||
        (field["plane_write_bytes"] + 0) != pixels / 2 ||
        (field["lookup_traffic_bytes"] + 0) != expected_lookup ||
        (field["bytes_per_batch"] + 0) != pixels / 2 ||
        (field["minimum_chip_traffic_bytes"] + 0) != expected_traffic) {
      reject("bad C2P traffic accounting on line " NR)
    }
    if (field["batch_iterations"] != "1" || field["samples"] != "3") {
      reject("bad C2P sampling contract on line " NR)
    }
  } else {
    reject("unknown case kind on line " NR)
  }
  ++cases
}

END {
  if (header_value["exclusive_graphics_benchmark_format"] != "1" ||
      header_value["benchmark"] != "chipram_c2p4") {
    reject("unsupported benchmark format")
  }
  if (!token(header_value["environment"]) ||
      header_value["timing_authority"] != "protocol_only" ||
      header_value["timing_scope"] != "exclusive_kernel_batch" ||
      header_value["timing_source"] != "eclock" ||
      header_value["interrupt_mode"] != "custom_intena_masked" ||
      header_value["source_memory"] != "chip" ||
      header_value["destination_memory"] != "chip" ||
      header_value["display_mode"] != "pal_256x256_dualpf_8plane" ||
      header_value["video_hz"] != "50" ||
      header_value["screen_owner"] != "intuition" ||
      header_value["publication"] != "direct_visible_pf1" ||
      header_value["sprite_load"] != "screen_managed" ||
      header_value["audio_load"] != "four_channel_period124_muted" ||
      header_value["blitter_load"] != "single_copy_a_to_d" ||
      header_value["interrupt_restore"] != "pass" ||
      header_value["dma_restore"] != "pass" ||
      header_value["checksum_algorithm"] != "fnv1a32") {
    reject("bad environment or execution metadata")
  }
  positive_header[1] = "eclock_hz"
  positive_header[2] = "raster_start_line"
  positive_header[3] = "dma_profile_count"
  positive_header[4] = "audio_channels"
  positive_header[5] = "blitter_copy_bytes"
  positive_header[6] = "dedicated_stack_bytes"
  positive_header[7] = "stack_boundary_reserve_bytes"
  positive_header[8] = "stack_guard_bytes"
  positive_header[9] = "stack_high_water_bytes"
  positive_header[10] = "initial_stack_bytes"
  positive_header[11] = "case_count"
  for (number = 1; number <= 11; ++number) {
    key = positive_header[number]
    if (!positive_decimal(header_value[key])) reject("bad header decimal " key)
  }
  if (!unsigned_decimal(header_value["timer_overhead_ticks"]) ||
      !unsigned_decimal(header_value["timer_discarded_samples"])) {
    reject("bad timer metadata")
  }
  if (header_value["raster_start_line"] != "32" ||
      header_value["dma_profile_count"] != "7" ||
      header_value["audio_channels"] != "4" ||
      header_value["blitter_copy_bytes"] != "32768") {
    reject("unexpected matrix contract")
  }
  high_water = header_value["stack_high_water_bytes"] + 0
  stack_size = header_value["dedicated_stack_bytes"] + 0
  if (high_water >= stack_size) {
    reject("dedicated stack exhausted")
  }
  for (profile in profile_known) {
    if (!(profile in profile_seen)) reject("missing DMA profile " profile)
  }
  if ((header_value["case_count"] + 0) != cases) {
    reject("case count mismatch")
  }
  if (!footer_seen || footer_line != NR) reject("missing final footer")
  if (failed) exit 1
}
' "$MAGI80_REPORT"

printf 'PASS exclusive graphics benchmark report schema and invariants\n'
