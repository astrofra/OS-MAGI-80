# MAGI-80 Exclusive Graphics Benchmark Plan

**Status:** Design contract; implementation pending

**Decision authority:** Only distributions produced by this harness on a physical stock PAL A1200 may select a graphics backend or certify a frame budget

## 1. Purpose

The existing hosted FS-UAE benchmark proves byte-exact output, target ABI execution, Chip-RAM allocation, report generation, and clean OS-managed teardown. It deliberately does not provide fine timing. AmigaOS remains scheduled, system interrupt handlers remain active, and every sample is preceded by `WaitTOF()` outside the measured bracket.

The exclusive harness separates two questions:

1. how fast a bounded kernel moves and transforms Chip-RAM data under a precisely declared DMA state;
2. whether the complete MAGI-80 runtime frame meets its deadline with the interrupt and DMA load it will actually own.

Both modes reuse the same canonical source data, destination decoder, checksums, case identifiers, and report schema as the hosted benchmark.

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

The exclusive harness MUST therefore:

- switch to or otherwise guarantee a dedicated stack sized from measured high-water use;
- place canaries at both guard boundaries and verify them before entry, after each batch, before restoration, and after restoration;
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
| Display state | blanked, eight-plane 256 × 256 active |
| DMA state | display only; display + Copper/sprites; display + blitter; representative display + sprites + Paula |

Every result reports bytes per iteration, iteration count, raw E-Clock ticks, bracket overhead, minimum/median/maximum, decimal MB/s, binary MiB/s, alignment, generated disassembly, display state, enabled DMA, and interrupt mode. Reads whose values do not affect an observable checksum are invalid because an optimizer may remove them.

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
interrupt_mode=masked_polled|magi80_level3
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

Until this gate passes, the hosted C2P4 matrix remains correctness and protocol evidence only.

## 9. Primary References

- [exec.library/Forbid autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node0353.html)
- [exec.library/Disable autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node034A.html)
- [exec.library/SetIntVector autodoc](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_3._guide/node0239.html)
- [Amiga ROM Kernel Reference Manual — Interrupt Servers](https://amigadev.elowar.com/read/ADCD_2.1/Libraries_Manual_guide/node030B.html)
- [Amiga Hardware Reference Manual — Interrupt Control Registers](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0164.html)
- [Amiga Hardware Reference Manual — Beam Position Detection](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node015E.html)
