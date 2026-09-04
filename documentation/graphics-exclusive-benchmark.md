# MAGI-80 Exclusive Graphics Benchmark Plan

**Status:** Version 2 of the blanked/active DMA matrix now includes explicit sprite fetch traffic, adaptive sustained fair-blitter contention, a hog-mode preemption burst, and autonomous 32-bit CIA timing; all 126 cases pass under FS-UAE; physical-hardware authority, transition stress, and runtime-frame timing remain pending

**Decision authority:** Only distributions produced by this harness on a physical stock PAL A1200 may select a graphics backend or certify a frame budget

## 1. Purpose

The hosted FS-UAE graphics benchmark proves byte-exact output, target ABI execution, Chip-RAM allocation, report generation, and clean OS-managed teardown. It deliberately does not provide fine timing. AmigaOS remains scheduled, system interrupt handlers remain active, and every sample is preceded by `WaitTOF()` outside the measured bracket.

The `exclusive_kernel_batch` successor now wraps both the raw Chip-RAM kernels and the real pair-LUT/mask32 68020 C2P4 kernels. It switches an Intuition-managed eight-plane dual-playfield screen between a blanked fixture and additive display/Copper/sprite/audio/blitter DMA profiles, while retaining the dedicated stack, interrupt masking, autonomous CIA timing, exact checksums, restoration checks, and machine-readable failure output. It does not yet replace the system Copper list, install the MAGI-80 runtime interrupt path, or model a complete runtime frame.

The exclusive harness separates two questions:

1. how fast a bounded kernel moves and transforms Chip-RAM data under a precisely declared DMA state;
2. whether the complete MAGI-80 runtime frame meets its deadline with the interrupt and DMA load it will actually own.

Graphics cases reuse the same canonical source data, destination decoder, checksums, case identifiers, and graphics report schema as the hosted benchmark. The raw-memory calibration uses a smaller dedicated report because it has no logical scene, draw stage, or planar output.

### 1.1 Implemented raw-memory smoke test

`tests/benchmark/chipram/` currently provides six reviewed 68020 kernels: sequential byte, word, longword, and four-way-unrolled longword writes; a four-way-unrolled longword copy; and a dependency-preserving four-way-unrolled read. Each sample processes four consecutive 8 KiB buffers inside one E-Clock bracket after an untimed warm-up. Source and destination allocations must be confirmed as Chip RAM.

The harness performs all allocation, initialization, device opening, output formatting, and cleanup in hosted mode. It then switches to a 16 KiB measured stack, calls `Forbid()`, and uses short matched `Disable()`/`Enable()` windows to save, mask, and restore Paula `INTENA` around each timed batch. The CPU interrupt mask is not held during the batch; no custom-chip source is enabled to reach an AmigaOS handler. `ReadEClock()` is the only called timing primitive, and there is no `WaitTOF()` or DOS operation in the exclusive section.

The stack allocation has 256-byte external canaries plus 256-byte declared reserves at both boundaries. The reserves are intentional: FS-UAE testing showed that Exec/GCC stack switching may touch boundary bookkeeping outside the downward-growing payload range even when the stack is healthy. The passing smoke report measured 78 bytes of payload-stack high water, preserved both external canaries, restored `INTENA` exactly, matched every checksum, and returned through normal cleanup.

Run and validate it with:

```sh
gmake chipram-benchmark-fs-uae
```

The report is written to `build/reports/chipram-fs-uae.txt`; kernel sizes and generated instructions are recorded in the adjacent size, symbol, and disassembly reports. It deliberately declares `timing_authority=protocol_only`. In particular, the initial headless FS-UAE smoke run captured display, sprite, and audio DMA as inactive. Its tick values cannot reproduce or refute the informal physical-machine figure near 6 MB/s.

### 1.2 Implemented graphics and DMA matrix

`tests/benchmark/exclusive-graphics/` opens a PAL 256 × 256 × 8 dual-playfield screen, verifies that all live planes are in Chip RAM, prepares the C2P oracle, allocates four Paula channels through `audio.device`, and acquires the blitter before entering `Forbid()`. Every state change is raster-polled to line 32 and performed inside a short `Disable()`/`Enable()` handover. Each batch restores and verifies the prior `DMACON` and `INTENA` bits; there is no `WaitTOF()`, allocation, DOS I/O, or report formatting in the exclusive section.

The seven profiles are additive so their deltas can be inspected:

| Profile | Display | Copper | Sprite | Audio | Blitter |
| --- | --- | --- | --- | --- | --- |
| `blanked` | off | off | off | off | off |
| `display` | on | off | off | off | off |
| `display_copper` | on | on | off | off | off |
| `display_copper_sprite` | on | on | seven controlled sprites | off | off |
| `display_copper_sprite_audio` | on | on | seven controlled sprites | four channels | off |
| `display_copper_sprite_audio_blitter_fair` | on | on | seven controlled sprites | four channels | adaptive 512 KiB, 2 MiB, or 4,194,176-byte fair A-to-D load |
| `display_copper_sprite_audio_blitter_hog` | on | on | seven controlled sprites | four channels | 512 KiB hog-mode A-to-D preemption burst |

Each profile runs the six raw kernels plus twelve C2P4 cases: Small, Medium, and Full viewports; packed4 and byte4 inputs; and the real pair-LUT and table-free mask32 68020 assembly cores. That produces 126 checked cases. C2P writes directly to the live PF1 planes and is compared with the hosted scalar oracle after the exclusive section. All measured CPU sources and lookup data are currently in Chip RAM, making this the stock-memory baseline.

The audio profile uses real four-channel DMA at period 124 with volume zero. Sprite channels 1 through 7 each display a 16-pixel-wide, 224-line `SimpleSprite`; their Chip-RAM streams account for a controlled minimum of 6,328 fetched bytes per video frame, in addition to the system pointer on channel 0. This is a deliberately heavy and repeatable fetch fixture, not yet the virtual-object scheduler or a multiplexing test.

The blitter source and destination are 128-byte Chip-RAM working sets. Negative modulos repeatedly revisit that row while AGA `BLTSIZV`/`BLTSIZH` describe a much longer logical transfer. Fair mode scales the load to 512 KiB for raw and Small cases, 2 MiB for Medium, and 4,194,176 bytes for Full; every accepted fair sample verifies that the blitter is busy immediately before and after the CPU kernel. Hog mode always launches a 512 KiB burst. Because BLITHOG withholds the Chip-RAM bus, CPU-side status reads occur only after the CPU regains the bus; hog start/end observations are therefore diagnostic rather than proof of overlap. Both modes remain contention controls, not C2P hybrids.

Version 2 also replaces `ReadEClock()` inside the long exclusive interval. The earlier implementation observed a lower-word wrap without the matching software-maintained upper-word advance after custom interrupts had remained masked for roughly 92 seconds. Before entering `Forbid()`, the harness now reserves Timer A and Timer B together from one CIA through `cia.resource`, disables their interrupt requests, runs Timer A from the E-clock and Timer B from Timer-A underflows, and reads the resulting descending 32-bit counter directly. It records the selected CIA and counter width, then stops and releases both timers during normal cleanup. Failure to reserve a complete pair is a controlled setup failure rather than an implicit timer conflict.

Run and validate the complete matrix with:

```sh
gmake exclusive-graphics-benchmark-fs-uae
```

The validated report is written to `build/reports/exclusive-graphics-fs-uae.txt`, with size, symbol, map, and disassembly artifacts beside the other benchmark reports. The validator checks the exact DMA profile mapping, sampling contracts, sprite and blitter workload invariants, traffic arithmetic, output hashes, timer plausibility, stack margin, state restoration, case count, and final footer. The passing version-2 FS-UAE run completed all 126 cases with 132 bytes of measured 16 KiB stack high water, a 24-tick timer-read overhead, and no discarded timer sample. Its `timing_authority=protocol_only` label is deliberate: the values prove execution and instrumentation, not A1200 performance.

### 1.3 Physical-test ADF

`gmake exclusive-graphics-test-adf` now produces a bootable OFS image plus a SHA-256 manifest in `build/distribution/`. The image is self-contained except for the Kickstart libraries and devices it deliberately uses, and contains no copied AmigaOS commands. Its physical build fixes the report path at compile time so a minimal Startup-Sequence does not depend on C-runtime command-line parsing.

The program first creates a closed `RESULT.TXT` containing `result=running`, before entering the exclusive section. On controlled completion it replaces that marker with either a full PASS report or failure diagnostics. An absent file therefore distinguishes startup or media-write trouble; a remaining `running` footer proves startup and writable media but classifies the run as incomplete. Raster waits and blitter waits are bounded so their controlled timeouts can unwind through the normal restoration path. The 68020 instruction cache is requested through `exec.library/CacheControl()` for a consistent benchmark state and its prior global state is restored during cleanup.

The exact writable version-2 ADF completes under the stock PAL A1200 FS-UAE profile. Its fixed 240-second controller window extracted and validated all 126 cases, preserved both restoration checks, reported 132 bytes of stack high water and a 24-tick timer-read overhead, and left no raster timeout or discarded timer sample. The final report is approximately 105 KiB. The writer restores all owned resources first, returns to the text display, and feeds `FWrite()` through a 32 KiB `SetVBuf()` buffer so OFS receives only a few large flushes. This integration result remains non-authoritative for timing. The ADF is deliberately labelled `real_hardware_candidate`; only returned reports from declared physical machines can advance the real-hardware gate.

The exact handoff, safety limit, observations, result-state meanings, return form, and extraction commands are in [MAGI-80 Physical A1200 Graphics Test](./physical-a1200-graphics-test.md).

## 2. Why `Forbid()` and `Disable()` Are Not Run Modes

`Forbid()` prevents task rescheduling while the current task remains ready, but it does not disable interrupts. Entering a wait state allows the system to run, so no DOS, device, allocation, or other potentially waiting call is permitted inside the exclusive section.

`Disable()` defers system interrupt processing and implies `Forbid()`. The Exec autodoc warns that holding it for more than approximately 250 microseconds disrupts vital system functions. It is therefore restricted to short atomic transition windows. It MUST NOT surround a benchmark batch or frame.

The long-running exclusive state is created by saving and controlling the relevant hardware interrupt sources and vectors, not by leaving Exec globally disabled.

## 3. Two Measurement Modes

### 3.1 `exclusive_kernel_batch`

This mode isolates a conversion, copy, fill, or raw memory kernel:

- the display is explicitly `blanked` or `active` according to the case;
- relevant custom-chip and CIA interrupt sources are saved and masked so unrelated OS handlers cannot run;
- reviewed synchronous exception capture remains installed; interactive emergency stop is added only when the exclusive input path is defined;
- raster position is polled directly for start alignment when alignment is required;
- one outer audited E-Clock/CIA read bracket covers enough back-to-back iterations to amortize timer-read overhead; the timing primitive must be proven non-waiting in this state;
- there is no `WaitTOF()`, DOS call, device I/O, allocation, or report formatting in the batch;
- source/destination roles rotate or are deterministically restored so repeated iterations perform equivalent work;
- output is checked after leaving exclusive mode.

This is the primary mode for raw Chip-RAM calibration and isolated C2P comparison. It is intentionally not a frame-budget claim.

### 3.2 `exclusive_runtime_frame`

This mode measures the representative runtime:

- MAGI-80 owns the Copper/display state and selected DMA channels;
- a reviewed minimal interrupt path services the required VBlank/raster and, in later cases, Paula timing sources;
- only explicitly enabled sources may reach the CPU;
- handlers acknowledge their hardware requests, preserve the documented registers, and update bounded preallocated state only;
- several consecutive frames are measured without returning to AmigaOS;
- frame distributions include rendering, C2P, blitter work/wait, publication, interrupt cost, and missed deadlines.

The Level-3 ownership mechanism needs its own reviewed spike. Exec interrupt servers are the cooperative integration path; `SetIntVector()` is documented for non-chained interrupt sources, while VBlank belongs to a server chain. A truly private Level-3 path must therefore save and restore the applicable CPU/vector and Paula state explicitly rather than misuse a chained-vector API. No direct-vector implementation becomes normative until it has passed repeated restoration and forced-failure tests.

## 4. Transactional State Machine

```text
HOSTED_PREPARE
    -> HOSTED_ORACLE_CHECK
    -> ACQUIRE_RESOURCES
    -> SAVE_STATE
    -> ATOMIC_ENTER
    -> EXCLUSIVE_KERNEL_BATCH | EXCLUSIVE_RUNTIME_FRAME
    -> ATOMIC_LEAVE
    -> RESTORE_STATE
    -> HOSTED_OUTPUT_CHECK
    -> WRITE_REPORT
```

Before `ATOMIC_ENTER`, the harness MUST:

- open every required library/device and allocate every buffer;
- create the oracle and initialize all deterministic inputs;
- reserve a dedicated, measured stack and fill its guard regions;
- wait for outstanding blits and acquire any required resource ownership;
- save all state that it will change, including applicable View/Copper, DMA, interrupt, CIA, palette, bitplane, sprite, blitter, and CPU-vector state;
- initialize a preallocated progress/crash record.

After all potentially waiting acquisition calls, entry uses one explicit `Forbid()` and later balances it with exactly one `Permit()`. `ATOMIC_ENTER` and `ATOMIC_LEAVE` may use matched `Disable()`/`Enable()` calls only for the minimum vector/register handover. Normal exclusive execution runs after that short window with only the intended hardware sources enabled. Nothing between `Forbid()` and `Permit()` may wait.

Restoration is reverse-order and idempotent where practical. A failure before entry unwinds through AmigaOS. A controlled kernel failure records the phase and leaves through the same restoration path. An arbitrary bus/address fault may still require a reboot; the benchmark must report that residual risk honestly.

## 5. Stack and Failure Diagnostics

The first 24-case hosted matrix exposed why timeouts are ambiguous: a temporary 2,976-byte local result matrix was compatible with exhausting the small default AmigaDOS process stack. Streaming one 124-byte result made the full matrix and teardown complete, but no exception was captured.

The complete exclusive harness MUST therefore:

- switch to or otherwise guarantee a dedicated stack sized from measured high-water use;
- place canaries outside declared boundary reserves and verify them before entry, after each batch, before restoration, and after restoration;
- keep large report/result arrays out of automatic storage;
- record the last entered and completed phase in preallocated memory;
- expose a raster/background-color phase marker for diagnosis on hardware and emulator;
- install only reviewed exception capture needed to record vector, SR, PC, SP, and the current phase before a controlled stop;
- emit `result=pass` only after output validation, state restoration, stack-canary validation, and cleanup.

The host controller classifies a missing footer as `incomplete`, never as a slow timing result. Emulator logs and the progress/crash record determine whether it can be refined to timeout, deadlock, exception, guard failure, or teardown failure.

## 6. Raw Chip-RAM Calibration Matrix

The reported informal figure near 6 MB/s is a hypothesis to reproduce, not a baseline constant. Each physical machine records model/revision, CPU, memory expansion absence, video standard, and harness checksum.

Minimum kernels:

| Family | Variants |
| --- | --- |
| Sequential write | `move.b`, `move.w`, `move.l`, unrolled longword stores |
| Sequential read | byte, word, longword with a dependency-preserving checksum |
| Mixed | copy, read/modify/write, four-source/four-plane C2P-like access |
| Alignment | naturally aligned plus every alignment accepted by the 68020 path |
| CPU-visible placement | Chip source/destination baseline; Fast source or CPU-only scratch to Chip destination on equipped machines |
| Display state | blanked, eight-plane 256 × 256 active |
| DMA state | display only; display + Copper/sprites; display + blitter; representative display + sprites + Paula |

Every physical-hardware result reports bytes per iteration, iteration count, raw E-Clock ticks, bracket overhead, minimum/median/maximum, decimal MB/s, binary MiB/s, alignment, source/destination memory domains, generated disassembly, display state, enabled DMA, and interrupt mode. Reads whose values do not affect an observable checksum are invalid because an optimizer may remove them. The implemented successor now records blanked/active and additive DMA profiles, explicit sprite traffic, adaptive fair/hog blitter traffic, and observable checksums. Rate formatting, byte/word read variants, read/modify/write kernels, misalignment cases, Fast-RAM placements, and physical distributions remain open.

## 7. Graphics Case Requirements

The first exclusive C2P set reuses all Small, Medium, and Full packed4/byte4 cases for:

- pair-LUT 68020;
- mask32 68020;
- the future genuine CPU/blitter merge;
- direct and safe-publication variants where applicable;
- full rebuild, aligned dirty regions, and no-change cases.

The report retains `timing_authority=real_hardware` only for a validated stock-machine run and adds at least:

```text
timing_scope=exclusive_kernel_batch|exclusive_runtime_frame
interrupt_mode=custom_intena_masked|masked_polled|magi80_level3
display_state=blanked|active
minimum_chip_traffic_bytes=<unsigned-decimal>
display_plane_fetch_bytes_per_video_frame=<unsigned-decimal>
video_hz=50
batch_iterations=<positive-decimal>
stack_guard_bytes=<positive-decimal>
stack_high_water_bytes=<positive-decimal>
```

Traffic fields remain payload lower bounds rather than measured bandwidth. The result must list what they omit.

## 8. Acceptance Gate

The exclusive harness is ready to inform architecture only when:

1. every candidate remains byte-identical to the portable oracle;
2. at least 100 hosted-to-exclusive-to-hosted transitions restore the display, interrupts, input, and OS operation;
3. forced failures at every transition phase either restore safely or produce the documented explicit reboot outcome;
4. stack guards remain intact and high-water margin is published;
5. blanked/active raw-memory results are repeatable and the generated kernels are inspected;
6. repeated case distributions, not single samples, are available from a physical stock PAL A1200;
7. the same report cannot accidentally label an FS-UAE run as `real_hardware`.

Until this gate passes, both the hosted and exclusive FS-UAE C2P4 matrices remain correctness and protocol evidence only.

The immediate implementation sequence is:

1. **Completed:** wrap the raw and C2P4 assembly kernels in blanked/active screen fixtures and the seven-profile DMA matrix, with guarded-stack, checksum, state-restoration, and report validation under FS-UAE;
2. **Completed under FS-UAE:** add seven explicit sprite payloads, adaptive sustained fair-blitter load, a separately identified hog-mode preemption burst, report-format-2 workload counters, and an exclusive 32-bit CIA timer that does not depend on system interrupt service;
3. distribute the refreshed candidate ADF using the physical-test handoff and collect repeated stock PAL A1200 reports; in parallel, complete missing raw read/mixed/alignment cases and optional Fast-source variants;
4. implement genuine C2P4 CPU/blitter merge candidates, dirty/no-change cases, and safe publication, preserving the same oracle and DMA profiles;
5. add the reviewed minimal Level-3 path for `exclusive_runtime_frame`, then stress at least 100 transitions and forced failures before the final physical selection run.

## 9. Primary References

- [exec.library/Forbid autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node0353.html)
- [exec.library/Disable autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node034A.html)
- [exec.library/SetIntVector autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_3._guide/node0239.html)
- [exec.library/ReadEClock autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node04FB.html)
- [cia.resource/AddICRVector autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node05B5.html)
- [Amiga Hardware Reference Manual — Timer B counting Timer A underflows](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0148.html)
- [exec.library/StackSwap autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node0382.html)
- [Amiga ROM Kernel Reference Manual — Interrupt Servers](https://amigadev.elowar.com/read/ADCD_2.1/Libraries_Manual_guide/node030B.html)
- [Amiga Hardware Reference Manual — Interrupt Control Registers](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0164.html)
- [Amiga Hardware Reference Manual — Beam Position Detection](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node015E.html)
