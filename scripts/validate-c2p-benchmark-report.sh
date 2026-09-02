#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MAGI80_REPORT="${1:-}"

if [ "$#" -ne 1 ] || [ ! -f "$MAGI80_REPORT" ]; then
  printf 'Usage: %s benchmark-report\n' "$0" >&2
  exit 1
fi

/usr/bin/awk '
function reject(message) {
  print "Invalid C2P benchmark report: " message > "/dev/stderr"
  failed = 1
}

BEGIN {
  expected["fb8"] = 65536
  expected["packed4_x2"] = 65536
  expected["byte4_x2"] = 131072
  expected["front_byte4_back_packed4"] = 98304
}

$0 == "benchmark_format=1" { format = 1 }
$0 == "backend=reference_c99" { backend = 1 }
$0 == "timing_source=eclock" { timer = 1 }
$0 == "timing_context=hosted_cooperative_display_dma_active" { context = 1 }
$0 == "source_memory=chip" { source_memory = 1 }
$0 == "destination_memory=screen_chip" { destination_memory = 1 }
$0 == "width=256" { width = 1 }
$0 == "height=256" { height = 1 }
$0 == "front_psets=16384" { psets = 1 }
$0 == "samples=1" { samples = 1 }
$0 == "result=pass" { passed = 1 }

/^eclock_hz=/ {
  split($0, item, "=")
  if (item[2] !~ /^[0-9]+$/ || item[2] == 0) reject("bad E-Clock rate")
  eclock = 1
}

/^timer_overhead_ticks=/ {
  split($0, item, "=")
  if (item[2] !~ /^[0-9]+$/) reject("bad timer overhead")
  overhead = 1
}

/^layout=/ {
  delete value
  for (field = 1; field <= NF; ++field) {
    split($field, item, "=")
    value[item[1]] = item[2]
  }
  name = value["layout"]
  if (!(name in expected)) {
    reject("unknown layout " name)
    next
  }
  if (seen[name]++) reject("duplicate layout " name)
  if (value["source_bytes"] != expected[name]) {
    reject("wrong source size for " name)
  }
  required[1] = "build_ticks"
  required[2] = "front_pset_ticks"
  required[3] = "c2p_ticks"
  required[4] = "phase_sum_ticks"
  required[5] = "checksum"
  for (number = 1; number <= 5; ++number) {
    key = required[number]
    if (value[key] !~ /^[0-9]+$/ || value[key] == 0) {
      reject("bad " key " for " name)
    }
  }
  sum = value["build_ticks"] + value["front_pset_ticks"] + value["c2p_ticks"]
  if (value["phase_sum_ticks"] != sum) reject("wrong phase sum for " name)
  if (checksums == 0) checksum = value["checksum"]
  else if (value["checksum"] != checksum) reject("checksum mismatch for " name)
  ++checksums
}

END {
  if (!format || !backend || !timer || !context || !source_memory ||
      !destination_memory || !width || !height || !psets || !samples ||
      !eclock || !overhead || !passed) reject("missing required header or footer")
  for (name in expected) {
    if (seen[name] != 1) reject("missing layout " name)
  }
  if (failed) exit 1
}
' "$MAGI80_REPORT"

printf 'PASS  C2P benchmark report schema, sizes, timings, and checksums\n'
