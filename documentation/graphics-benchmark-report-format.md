# MAGI-80 Graphics Benchmark Report Format

**Status:** Version 1 schema and native validator implemented

**Role:** Common correctness, timing, memory, workload, and DMA metadata for all Phase 0 graphics backends

## 1. Design Goals

Every candidate must report the same facts before its timings can be compared. The format is deliberately line-oriented so an Amiga C program can emit it with little code and host scripts can validate it without a JSON library.

Version 1 separates correctness from timing authority:

- an FS-UAE report uses `timing_authority=protocol_only` and may prove automation, phase coverage, and output equivalence;
- only a controlled stock-A1200 run uses `timing_authority=real_hardware` for a performance decision.

Changing DMA state requires a separate report. A report must not mix cases measured under different display, sprite, or audio contention.

## 2. Encoding Rules

- The file is ASCII-compatible text with one record per line and a final newline.
- Blank lines and comments are not allowed.
- Header fields appear exactly once and in the order defined below.
- A case is one space-separated line of `key=value` fields. `case` is first and `result=pass` is last.
- Keys match `[a-z][a-z0-9_]*`.
- Values match `[a-z0-9_.-]+`; required numeric values use unsigned decimal notation.
- Required fields appear exactly once. Additional fields are allowed for benchmark-specific detail.
- Case identifiers are unique within the report.
- The final record is exactly `result=pass`; no content follows it.

## 3. Required Header

The first 13 lines have this exact order:

```text
graphics_benchmark_format=1
benchmark=<token>
environment=<token>
timing_authority=protocol_only|real_hardware
timing_source=eclock
eclock_hz=<positive-decimal>
timer_overhead_ticks=<unsigned-decimal>
canonical_format=palette_identity_u8
checksum_algorithm=fnv1a32
display_dma=active|inactive
sprite_dma=active|inactive
audio_dma=active|inactive
case_count=<positive-decimal>
```

`benchmark` identifies the suite, such as `c2p4`. `environment` identifies a reproducible configuration rather than free-form prose, for example `fs_uae_a1200_pal` or `stock_a1200_pal_unit1`. Exact ROM, executable, compiler, hardware, and harness revisions should be retained in the surrounding test manifest.

`timer_overhead_ticks` is the minimum observed overhead of the timing bracket used by that executable. Raw E-Clock ticks are authoritative; derived milliseconds are presentation-only and should be generated after validation.

## 4. Required Case Record

Every case line contains these fields:

| Field | Meaning |
| --- | --- |
| `case` | Unique stable case identifier |
| `backend` | Implementation identifier, including relevant algorithm class |
| `workload` | Stable logical-scene/workload identifier |
| `width`, `height` | Active pixel-viewport dimensions |
| `samples` | Number of measured complete samples after warm-up |
| `memory_bytes` | Peak bytes owned by the graphics case for sources, destinations, scratch space, and persistent backend state |
| `dirty_bytes`, `dirty_regions` | Logical changed-byte and region counts presented to the backend |
| `objects`, `fallback_objects` | Submitted virtual objects and the subset rendered through fallback |
| `frame_budget_ticks` | Deadline for the selected Standard or Turbo profile at the reported E-Clock rate |
| `source_median_ticks` | Median source construction or preparation time |
| `draw_median_ticks` | Median logical drawing and native-planar work |
| `cpu_conversion_median_ticks` | Median CPU work specific to chunky-to-planar conversion |
| `blitter_median_ticks` | Median elapsed region in which scheduled blitter conversion/render work is active |
| `blitter_wait_median_ticks` | Median CPU time blocked waiting for blitter completion or ownership |
| `publication_median_ticks` | Median safe-display publication or pointer-swap work |
| `total_min_ticks`, `total_median_ticks`, `total_max_ticks` | Distribution summary for the complete measured frame pipeline |
| `oracle_checksum` | FNV-1a checksum of the reference compositor output |
| `canonical_checksum` | FNV-1a checksum after decoding the candidate output back to canonical identities |
| `deadline_misses` | Samples whose complete total exceeded `frame_budget_ticks` |
| `result` | Must be `pass` |

Zero is valid for a phase, dirty count, object count, fallback count, deadline count, or measured tick count. Width, height, samples, memory footprint, and frame budget must be positive. The validator also requires:

- `total_min_ticks <= total_median_ticks <= total_max_ticks`;
- `deadline_misses <= samples`;
- `fallback_objects <= objects`;
- both checksums to be unsigned 32-bit decimals and exactly equal.

The phase medians are diagnostic and may overlap, especially when CPU and blitter work run concurrently. They therefore do not have to sum to `total_median_ticks`. Total time must be measured independently around the complete pipeline.

Benchmark-specific fields such as `source_layout`, `dirty_percent`, `blitter_priority`, `scroll_margin`, `sprite_channels`, or `audio_channels` may be appended before `result=pass`. Once a field affects comparisons across suites, it should be promoted in a later schema revision rather than inferred from a case name.

Current graphics producers SHOULD additionally emit:

| Field | Meaning |
| --- | --- |
| `timing_scope` | `hosted_cooperative` or `exclusive_runtime`; the former cannot authorize a performance decision |
| `lookup_traffic_bytes` | Payload bytes read from conversion lookup tables during the complete sample |
| `minimum_chip_traffic_bytes` | Conservative payload lower bound for explicitly modeled CPU/blitter source, edit, intermediate, and destination traffic |
| `display_plane_fetch_bytes_per_video_frame` | Active display-plane payload, kept separate from the workload lower bound |
| `video_hz` | Video refresh rate associated with the display fetch value |

Traffic accounting is diagnostic, not a bandwidth measurement. A producer must document included and omitted traffic and must not infer available bus bandwidth from these fields alone.

## 5. Canonical Checksum

Both checksum fields cover the complete 256 × 256 canonical image in top-to-bottom, left-to-right order, regardless of viewport size. Start with `2166136261`; for each canonical byte, XOR it into the low eight bits and multiply by `16777619` modulo 2^32.

The reference side comes from the three-layer compositor. The candidate side comes from the AGA playfield decoder plus, once object tests begin, the hardware-sprite output adapter. Timing is invalid unless these checksums match. Collision-sensitive development failures should retain the two canonical images and perform an exact byte comparison on the host.

## 6. Example

```text
graphics_benchmark_format=1
benchmark=c2p4
environment=fs_uae_a1200_pal
timing_authority=protocol_only
timing_source=eclock
eclock_hz=709379
timer_overhead_ticks=11
canonical_format=palette_identity_u8
checksum_algorithm=fnv1a32
display_dma=active
sprite_dma=inactive
audio_dma=inactive
case_count=1
case=byte4_160x128 backend=scalar_c99 workload=full_frame width=160 height=128 samples=9 memory_bytes=30720 dirty_bytes=20480 dirty_regions=1 objects=0 fallback_objects=0 frame_budget_ticks=14187 source_median_ticks=120 draw_median_ticks=900 cpu_conversion_median_ticks=3000 blitter_median_ticks=0 blitter_wait_median_ticks=0 publication_median_ticks=40 total_min_ticks=4210 total_median_ticks=4250 total_max_ticks=4301 oracle_checksum=305419896 canonical_checksum=305419896 deadline_misses=0 source_layout=byte4 result=pass
result=pass
```

The numbers above illustrate the syntax only; they are not MAGI-80 measurements.

## 7. Validation

Validate one producer report with:

```sh
./scripts/validate-graphics-benchmark-report.sh path/to/report.txt
```

Run the schema's positive and negative fixtures with:

```sh
gmake graphics-report-test
```

The test proves acceptance of the version 1 contract and rejection of wrong case counts, unordered timing distributions, and oracle checksum mismatches. `gmake check` includes it.

The earlier two-layer C2P harness retains its legacy `benchmark_format=1` report and validator for reproducibility. The `c2p4` producer uses this graphics-wide schema for its C99, pair-LUT and mask32 68020 assembly, and staged blitter-publication cases. Future planar, scroll, object, combined-scene, and genuine hybrid C2P benchmarks must use the same schema.

An external controller timeout is not a valid case record. A successful run must emit the final `result=pass` only after cleanup. An exclusive harness should also preserve bounded phase/progress markers and explicit failure state so a missing footer can be classified as slow execution, deadlock, crash, stack corruption, or teardown failure rather than guessed from elapsed wall time.
