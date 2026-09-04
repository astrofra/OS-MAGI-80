#!/usr/bin/env bash

set -euo pipefail
export LC_ALL=C
export LANG=C

MIGA80_REPORT="${1:-}"

if [ "$#" -ne 1 ] || [ ! -f "$MIGA80_REPORT" ]; then
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
  header_count = 42
  header_name[1] = "exclusive_graphics_benchmark_format"
  header_name[2] = "benchmark"
  header_name[3] = "environment"
  header_name[4] = "timing_authority"
  header_name[5] = "timing_scope"
  header_name[6] = "timing_source"
  header_name[7] = "exec_version"
  header_name[8] = "exec_revision"
  header_name[9] = "attention_flags"
  header_name[10] = "power_supply_hz"
  header_name[11] = "available_chip_bytes_before_setup"
  header_name[12] = "available_fast_bytes_before_setup"
  header_name[13] = "detected_stock_constraints"
  header_name[14] = "initial_instruction_cache"
  header_name[15] = "benchmark_instruction_cache"
  header_name[16] = "raster_timeout_count"
  header_name[17] = "eclock_hz"
  header_name[18] = "timer_overhead_ticks"
  header_name[19] = "timer_discarded_samples"
  header_name[20] = "interrupt_mode"
  header_name[21] = "source_memory"
  header_name[22] = "destination_memory"
  header_name[23] = "display_mode"
  header_name[24] = "video_hz"
  header_name[25] = "screen_owner"
  header_name[26] = "publication"
  header_name[27] = "sprite_load"
  header_name[28] = "audio_load"
  header_name[29] = "blitter_load"
  header_name[30] = "raster_start_line"
  header_name[31] = "dma_profile_count"
  header_name[32] = "audio_channels"
  header_name[33] = "blitter_copy_bytes"
  header_name[34] = "dedicated_stack_bytes"
  header_name[35] = "stack_boundary_reserve_bytes"
  header_name[36] = "stack_guard_bytes"
  header_name[37] = "stack_high_water_bytes"
  header_name[38] = "initial_stack_bytes"
  header_name[39] = "interrupt_restore"
  header_name[40] = "dma_restore"
  header_name[41] = "checksum_algorithm"
  header_name[42] = "case_count"
  header_name[43] = "controlled_sprite_count"
  header_name[44] = "controlled_sprite_height"
  header_name[45] = "controlled_sprite_fetch_bytes_per_video_frame"
  header_name[46] = "blitter_working_set_bytes"
  header_name[47] = "blitter_rows"
  header_name[48] = "exclusive_timer_resource"
  header_name[49] = "exclusive_timer_counter_bits"
  header_name[50] = "raw_core_kernel_count"
  header_name[51] = "raw_extended_kernel_count"
  header_name[52] = "raw_extended_dma_profile_count"
  header_name[53] = "fast_matrix"
  header_name[54] = "fast_matrix_dma_profile_count"
  header_name[55] = "fast_case_count"
  header_name[56] = "stack_memory"
  header_name[57] = "code_memory"
  header_name[58] = "raw_result_count"
  header_name[59] = "c2p_result_count"
  header_name[60] = "baseline_case_count"

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
  common[23] = "minimum_controlled_sprite_fetch_bytes_per_video_frame"
  common[24] = "blitter_busy_at_kernel_end_samples"
  common[25] = "blitter_copy_bytes"
  common[26] = "blitter_busy_at_kernel_start_samples"
  common[27] = "blitter_launch_samples"
  common[28] = "source_memory"
  common[29] = "destination_memory"
  common[30] = "lookup_memory"
  common[31] = "source_offset"
  common[32] = "destination_offset"
  common[33] = "minimum_total_memory_traffic_bytes"

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
  numeric[13] = "minimum_controlled_sprite_fetch_bytes_per_video_frame"
  numeric[14] = "blitter_busy_at_kernel_end_samples"
  numeric[15] = "blitter_copy_bytes"
  numeric[16] = "blitter_busy_at_kernel_start_samples"
  numeric[17] = "blitter_launch_samples"
  numeric[18] = "source_offset"
  numeric[19] = "destination_offset"
  numeric[20] = "minimum_total_memory_traffic_bytes"

  expect_dma("blanked", "inactive", "inactive", "inactive", "inactive", "inactive")
  expect_dma("display", "active", "inactive", "inactive", "inactive", "inactive")
  expect_dma("display_copper", "active", "active", "inactive", "inactive", "inactive")
  expect_dma("display_copper_sprite", "active", "active", "active", "inactive", "inactive")
  expect_dma("display_copper_sprite_audio", "active", "active", "active", "active", "inactive")
  expect_dma("display_copper_sprite_audio_blitter_fair", "active", "active", "active", "active", "busy_fair")
  expect_dma("display_copper_sprite_audio_blitter_hog", "active", "active", "active", "active", "busy_hog")
}

NR == 1 {
  value = parse_header(header_name[NR])
  header_value[header_name[NR]] = value
  report_format = value + 0
  if (value == "3") {
    header_count = 60
  } else if (value == "2") {
    header_count = 49
  } else if (value != "1") {
    reject("unsupported benchmark format")
  }
  next
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
  common_count = report_format == 3 ? 33 : (report_format == 2 ? 27 : 22)
  for (number = 1; number <= common_count; ++number) {
    require_field(common[number])
  }
  if (field["case"] in case_seen) reject("duplicate case " field["case"])
  case_seen[field["case"]] = 1

  numeric_count = report_format == 3 ? 20 : (report_format == 2 ? 17 : 12)
  for (number = 1; number <= numeric_count; ++number) {
    key = numeric[number]
    if (!unsigned_decimal(field[key])) reject("bad decimal " key " on line " NR)
  }
  if (!positive_decimal(field["batch_iterations"]) ||
      !positive_decimal(field["samples"]) ||
      !positive_decimal(field["bytes_per_batch"]) ||
      (report_format < 3 &&
       !positive_decimal(field["minimum_chip_traffic_bytes"])) ||
      (report_format == 3 &&
       !positive_decimal(field["minimum_total_memory_traffic_bytes"])) ||
      !positive_decimal(field["frame_budget_ticks"])) {
    reject("zero required magnitude on line " NR)
  }
  if ((field["minimum_ticks"] + 0) > (field["median_ticks"] + 0) ||
      (field["median_ticks"] + 0) > (field["maximum_ticks"] + 0)) {
    reject("unordered timing values on line " NR)
  }
  maximum_plausible_seconds = report_format >= 2 ? 10 : 2
  maximum_plausible_ticks = (header_value["eclock_hz"] + 0) * maximum_plausible_seconds
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
    if (report_format >= 2) {
      expected_sprite_fetch = profile_sprite[profile] == "active" ? 6328 : 0
      actual_sprite_fetch = field["minimum_controlled_sprite_fetch_bytes_per_video_frame"] + 0
      actual_start_busy_samples = field["blitter_busy_at_kernel_start_samples"] + 0
      actual_busy_samples = field["blitter_busy_at_kernel_end_samples"] + 0
      expected_launch_samples = profile_blitter[profile] == "inactive" ? 0 : field["samples"] + 0
      actual_launch_samples = field["blitter_launch_samples"] + 0
      expected_case_blitter_bytes = 0
      if (profile_blitter[profile] != "inactive") {
        if (profile_blitter[profile] == "busy_hog" ||
            field["kind"] == "raw" || field["width"] == "160") {
          expected_case_blitter_bytes = 524288
        } else if (field["width"] == "192") {
          expected_case_blitter_bytes = 2097152
        } else if (field["width"] == "256") {
          expected_case_blitter_bytes = 4194176
        }
      }
      fair_start_mismatch = profile_blitter[profile] == "busy_fair" && actual_start_busy_samples != (field["samples"] + 0)
      inactive_start_mismatch = profile_blitter[profile] == "inactive" && actual_start_busy_samples != 0
      hog_start_invalid = profile_blitter[profile] == "busy_hog" && actual_start_busy_samples > (field["samples"] + 0)
      fair_end_mismatch = profile_blitter[profile] == "busy_fair" && actual_busy_samples != (field["samples"] + 0)
      inactive_end_mismatch = profile_blitter[profile] == "inactive" && actual_busy_samples != 0
      hog_end_invalid = profile_blitter[profile] == "busy_hog" && actual_busy_samples > (field["samples"] + 0)
      if (actual_sprite_fetch != expected_sprite_fetch ||
          actual_launch_samples != expected_launch_samples ||
          fair_start_mismatch || inactive_start_mismatch || hog_start_invalid ||
          fair_end_mismatch || inactive_end_mismatch || hog_end_invalid ||
          (field["blitter_copy_bytes"] + 0) != expected_case_blitter_bytes) {
        reject("contention metadata does not match profile " profile " on line " NR)
      }
    }
  }

  if (report_format == 3) {
    if ((field["source_memory"] != "none" &&
         field["source_memory"] != "chip" &&
         field["source_memory"] != "fast") ||
        (field["destination_memory"] != "none" &&
         field["destination_memory"] != "chip") ||
        (field["lookup_memory"] != "none" &&
         field["lookup_memory"] != "chip" &&
         field["lookup_memory"] != "fast") ||
        (field["source_offset"] + 0) > 3 ||
        (field["destination_offset"] + 0) > 3) {
      reject("bad memory placement metadata on line " NR)
    }
    if (field["source_memory"] == "fast") {
      ++fast_cases
      if (profile != "blanked" &&
          profile != "display_copper_sprite_audio_blitter_fair") {
        reject("Fast-assisted case uses an unexpected DMA profile on line " NR)
      }
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
    batch_bytes = field["bytes_per_batch"] + 0
    if (report_format == 3) {
      raw_operation = field["operation"]
      source_memory = field["source_memory"]
      destination_memory = field["destination_memory"]
      lookup_memory = field["lookup_memory"]
      expected_total_traffic = 0
      expected_chip_traffic = 0
      if (raw_operation == "write") {
        if (source_memory != "none" ||
            destination_memory != "chip" ||
            lookup_memory != "none" ||
            field["source_offset"] != "0") {
          reject("bad raw write placement on line " NR)
        }
        expected_total_traffic = batch_bytes
        expected_chip_traffic = batch_bytes
      } else if (raw_operation == "read") {
        if ((source_memory != "chip" && source_memory != "fast") ||
            destination_memory != "none" ||
            lookup_memory != "none" ||
            field["destination_offset"] != "0") {
          reject("bad raw read placement on line " NR)
        }
        expected_total_traffic = batch_bytes
        expected_chip_traffic = source_memory == "chip" ? batch_bytes : 0
      } else if (raw_operation == "copy") {
        if ((source_memory != "chip" && source_memory != "fast") ||
            destination_memory != "chip" ||
            lookup_memory != "none") {
          reject("bad raw copy placement on line " NR)
        }
        expected_total_traffic = batch_bytes * 2
        expected_chip_traffic = batch_bytes + (source_memory == "chip" ? batch_bytes : 0)
      } else if (raw_operation == "read_modify_write") {
        if (source_memory != "none" ||
            destination_memory != "chip" ||
            lookup_memory != "none" ||
            field["source_offset"] != "0") {
          reject("bad raw read-modify-write placement on line " NR)
        }
        expected_total_traffic = batch_bytes * 2
        expected_chip_traffic = expected_total_traffic
      } else {
        reject("unknown raw operation on line " NR)
      }
      actual_total_traffic = field["minimum_total_memory_traffic_bytes"] + 0
      actual_chip_traffic = field["minimum_chip_traffic_bytes"] + 0
      if (actual_total_traffic != expected_total_traffic ||
          actual_chip_traffic != expected_chip_traffic) {
        reject("bad raw traffic accounting on line " NR)
      }
    } else if ((field["minimum_chip_traffic_bytes"] + 0) < batch_bytes) {
      reject("raw traffic smaller than payload on line " NR)
    }
    ++raw_cases
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
    if (report_format == 3) {
      if ((field["source_memory"] != "chip" &&
           field["source_memory"] != "fast") ||
          field["destination_memory"] != "chip" ||
          field["source_offset"] != "0" ||
          field["destination_offset"] != "0" ||
          (field["backend"] == "pair_lut_m68k" &&
           field["lookup_memory"] != field["source_memory"]) ||
          (field["backend"] == "mask32_m68k" &&
           field["lookup_memory"] != "none")) {
        reject("bad C2P memory placement on line " NR)
      }
      expected_chip_traffic = pixels / 2
      if (field["source_memory"] == "chip") {
        expected_chip_traffic += expected_source
      }
      if (field["lookup_memory"] == "chip") {
        expected_chip_traffic += expected_lookup
      }
    } else {
      expected_chip_traffic = expected_traffic
    }
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
        (field["minimum_chip_traffic_bytes"] + 0) != expected_chip_traffic ||
        (report_format == 3 &&
         (field["minimum_total_memory_traffic_bytes"] + 0) != expected_traffic)) {
      reject("bad C2P traffic accounting on line " NR)
    }
    if (field["batch_iterations"] != "1" || field["samples"] != "3") {
      reject("bad C2P sampling contract on line " NR)
    }
    ++c2p_cases
  } else {
    reject("unknown case kind on line " NR)
  }
  ++cases
}

END {
  if ((report_format != 1 && report_format != 2 && report_format != 3) ||
      header_value["benchmark"] != "chipram_c2p4") {
    reject("unsupported benchmark format")
  }
  authority = header_value["timing_authority"]
  if (!token(header_value["environment"]) ||
      (authority != "protocol_only" &&
       authority != "real_hardware_candidate") ||
      header_value["timing_scope"] != "exclusive_kernel_batch" ||
      (header_value["initial_instruction_cache"] != "active" &&
       header_value["initial_instruction_cache"] != "inactive") ||
      header_value["benchmark_instruction_cache"] != "active" ||
      header_value["interrupt_mode"] != "custom_intena_masked" ||
      header_value["source_memory"] != "chip" ||
      header_value["destination_memory"] != "chip" ||
      header_value["display_mode"] != "pal_256x256_dualpf_8plane" ||
      header_value["video_hz"] != "50" ||
      header_value["screen_owner"] != "intuition" ||
      header_value["publication"] != "direct_visible_pf1" ||
      header_value["audio_load"] != "four_channel_period124_muted" ||
      header_value["interrupt_restore"] != "pass" ||
      header_value["dma_restore"] != "pass" ||
      header_value["checksum_algorithm"] != "fnv1a32") {
    reject("bad environment or execution metadata")
  }
  if (report_format == 1 &&
      (header_value["timing_source"] != "eclock" ||
       header_value["sprite_load"] != "screen_managed" ||
       header_value["blitter_load"] != "single_copy_a_to_d")) {
    reject("bad version 1 contention metadata")
  }
  if (report_format >= 2 &&
      (header_value["timing_source"] != "cia_cascade_32" ||
       header_value["sprite_load"] != "seven_simple_16x224_plus_system_pointer" ||
       header_value["blitter_load"] != "adaptive_fair_overlap_and_hog_burst_a_to_d" ||
       (header_value["exclusive_timer_resource"] != "ciaa" &&
        header_value["exclusive_timer_resource"] != "ciab") ||
       header_value["exclusive_timer_counter_bits"] != "32")) {
    reject("bad version 2+ contention metadata")
  }
  if (authority == "real_hardware_candidate" &&
      header_value["environment"] != "physical_a1200_pal_candidate") {
    reject("hardware-candidate authority has the wrong environment")
  }
  if (header_value["detected_stock_constraints"] != "pass" &&
      header_value["detected_stock_constraints"] != "fail") {
    reject("bad detected stock constraints metadata")
  }
  if (header_value["raster_timeout_count"] != "0") {
    reject("raster synchronization timed out")
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
  positive_header[12] = "exec_version"
  positive_header[13] = "power_supply_hz"
  positive_header[14] = "available_chip_bytes_before_setup"
  for (number = 1; number <= 14; ++number) {
    key = positive_header[number]
    if (!positive_decimal(header_value[key])) reject("bad header decimal " key)
  }
  if (report_format >= 2) {
    version2_positive[1] = "controlled_sprite_count"
    version2_positive[2] = "controlled_sprite_height"
    version2_positive[3] = "controlled_sprite_fetch_bytes_per_video_frame"
    version2_positive[4] = "blitter_working_set_bytes"
    version2_positive[5] = "blitter_rows"
    version2_positive[6] = "exclusive_timer_counter_bits"
    for (number = 1; number <= 6; ++number) {
      key = version2_positive[number]
      if (!positive_decimal(header_value[key])) {
        reject("bad version 2+ header decimal " key)
      }
    }
  }
  if (!unsigned_decimal(header_value["timer_overhead_ticks"]) ||
      !unsigned_decimal(header_value["timer_discarded_samples"]) ||
      !unsigned_decimal(header_value["raster_timeout_count"]) ||
      !unsigned_decimal(header_value["exec_revision"]) ||
      !unsigned_decimal(header_value["attention_flags"]) ||
      !unsigned_decimal(header_value["available_fast_bytes_before_setup"])) {
    reject("bad timer metadata")
  }
  expected_blitter_bytes = report_format >= 2 ? 4194176 : 32768
  if (header_value["raster_start_line"] != "32" ||
      header_value["dma_profile_count"] != "7" ||
      header_value["audio_channels"] != "4" ||
      (header_value["blitter_copy_bytes"] + 0) != expected_blitter_bytes) {
    reject("unexpected matrix contract")
  }
  if (report_format >= 2 &&
      (header_value["controlled_sprite_count"] != "7" ||
       header_value["controlled_sprite_height"] != "224" ||
       header_value["controlled_sprite_fetch_bytes_per_video_frame"] != "6328" ||
       header_value["blitter_working_set_bytes"] != "128" ||
       header_value["blitter_rows"] != "32767")) {
    reject("unexpected version 2+ contention contract")
  }
  if (report_format == 3) {
    version3_positive[1] = "raw_core_kernel_count"
    version3_positive[2] = "raw_extended_kernel_count"
    version3_positive[3] = "raw_extended_dma_profile_count"
    version3_positive[4] = "fast_matrix_dma_profile_count"
    version3_positive[5] = "raw_result_count"
    version3_positive[6] = "c2p_result_count"
    version3_positive[7] = "baseline_case_count"
    for (number = 1; number <= 7; ++number) {
      key = version3_positive[number]
      if (!positive_decimal(header_value[key])) {
        reject("bad version 3 header decimal " key)
      }
    }
    if (!unsigned_decimal(header_value["fast_case_count"])) {
      reject("bad version 3 header decimal fast_case_count")
    }
    fast_state = header_value["fast_matrix"]
    if (header_value["raw_core_kernel_count"] != "6" ||
        header_value["raw_extended_kernel_count"] != "26" ||
        header_value["raw_extended_dma_profile_count"] != "3" ||
        header_value["fast_matrix_dma_profile_count"] != "2" ||
        header_value["baseline_case_count"] != "204") {
      reject("unexpected version 3 matrix contract")
    }
    if ((header_value["stack_memory"] != "chip" &&
         header_value["stack_memory"] != "fast") ||
        (header_value["code_memory"] != "chip" &&
         header_value["code_memory"] != "fast" &&
         header_value["code_memory"] != "other")) {
      reject("bad version 3 execution placement")
    }
    if (fast_state == "active") {
      if ((header_value["available_fast_bytes_before_setup"] + 0) == 0 ||
          header_value["detected_stock_constraints"] != "fail" ||
          header_value["stack_memory"] != "fast" ||
          header_value["fast_case_count"] != "56" ||
          header_value["raw_result_count"] != "152" ||
          header_value["c2p_result_count"] != "108" ||
          header_value["case_count"] != "260") {
        reject("inconsistent active Fast matrix metadata")
      }
    } else if (fast_state == "not_present") {
      if ((header_value["available_fast_bytes_before_setup"] + 0) != 0 ||
          header_value["detected_stock_constraints"] != "pass" ||
          header_value["stack_memory"] != "chip" ||
          header_value["fast_case_count"] != "0" ||
          header_value["raw_result_count"] != "120" ||
          header_value["c2p_result_count"] != "84" ||
          header_value["case_count"] != "204") {
        reject("inconsistent absent Fast matrix metadata")
      }
    } else if (fast_state == "insufficient") {
      if ((header_value["available_fast_bytes_before_setup"] + 0) == 0 ||
          header_value["detected_stock_constraints"] != "fail" ||
          header_value["stack_memory"] != "chip" ||
          header_value["fast_case_count"] != "0" ||
          header_value["raw_result_count"] != "120" ||
          header_value["c2p_result_count"] != "84" ||
          header_value["case_count"] != "204") {
        reject("inconsistent insufficient Fast matrix metadata")
      }
    } else {
      reject("unknown Fast matrix state")
    }
    if ((header_value["fast_case_count"] + 0) != fast_cases ||
        (header_value["raw_result_count"] + 0) != raw_cases ||
        (header_value["c2p_result_count"] + 0) != c2p_cases ||
        (header_value["baseline_case_count"] + 0) != cases - fast_cases ||
        raw_cases + c2p_cases != cases) {
      reject("version 3 case accounting mismatch")
    }
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
' "$MIGA80_REPORT"

printf 'PASS exclusive graphics benchmark report schema and invariants\n'
