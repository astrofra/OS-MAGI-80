# MAGI-80 Physical A1200 Graphics Test

**Status:** Physical-hardware candidate. The ADF boots in FS-UAE and creates its on-disk `running` marker. The complete 126-case binary passes when launched through the project's host-backed FS-UAE volume, but completion from the writable ADF path has not yet been observed within the automated 240-second window. A physical result is evidence for this investigation, not a release certification by itself.

## 1. What to Send

Send these three files together:

- `build/distribution/magi80-exclusive-graphics-test.adf`;
- `build/distribution/magi80-exclusive-graphics-test.manifest.txt`;
- this document.

Build and inspect them with:

```sh
make exclusive-graphics-test-adf
make exclusive-graphics-test-adf-inspect
```

The manifest records the exact ADF and executable SHA-256 digests. Do not add files to the image after building it. The ADF is OFS, is bootable, and contains no AmigaOS command or ROM files; it calls the libraries and devices supplied by the machine's Kickstart ROM.

Keep the generated ADF as an untouched master. Make one writable copy for every run, for example:

```sh
cp build/distribution/magi80-exclusive-graphics-test.adf friend-run-01.adf
cp build/distribution/magi80-exclusive-graphics-test.adf friend-run-02.adf
cp build/distribution/magi80-exclusive-graphics-test.adf friend-run-03.adf
```

Three cold runs are preferred. Never reuse or overwrite a returned run image before its result has been extracted.

## 2. Requested Machine

The primary test configuration is:

- PAL Amiga 1200;
- stock 68EC020 CPU;
- 2 MiB Chip RAM;
- no Fast RAM;
- no accelerator or other CPU replacement;
- Kickstart 3.0 or 3.1;
- a working internal drive, Gotek, or equivalent device that permits writes to the selected ADF.

Disconnect optional accelerators and memory expansions when practical. If this is not practical, leave them installed and record them accurately; the report's `detected_stock_constraints` field will normally be `fail`, and the run must not be treated as a stock-machine result.

The image must be writable. Preserve the master first because the benchmark writes `RESULT.TXT` into its own filesystem. When using a real floppy, write a fresh copy of the master ADF before each run and read the complete disk back to a new ADF afterward. When using a Gotek, make sure its firmware is configured to write changes back to the image rather than discard them.

## 3. Before Each Run

Record the following before switching the machine on:

```text
Run identifier:
Date:
Tester:
Amiga model and motherboard revision, if known:
Kickstart version shown by the machine:
CPU or accelerator:
Chip RAM:
Fast RAM:
Other expansions:
Boot medium and firmware, if applicable:
Video output and display:
Cold boot or warm reset:
ADF SHA-256 before the run:
```

Use a stopwatch or phone timer. If possible, arrange a camera so that both the display and the drive activity LED are visible.

## 4. Test Procedure

1. Insert or select a fresh writable run copy.
2. Cold-boot the Amiga from that copy.
3. Start the timer when the drive begins booting.
4. Do not press keys or move the mouse while the benchmark is running.
5. Expect an initial text message, followed by screen blanking, size changes, or flicker while DMA profiles are changed. Audio is deliberately muted; audible noise is unexpected and should be reported.
6. Wait for the text screen to return with one of the explicit verdicts below.
7. Stop the timer when the verdict appears.
8. Photograph the final screen. Wait until the drive LED has been inactive for at least five seconds before ejecting the disk, deselecting the ADF, resetting, or powering off.
9. Preserve the modified disk or ADF under its run identifier.

A successful run prints:

```text
MAGI-80 BENCHMARK RESULT: PASS
```

A controlled failure prints:

```text
MAGI-80 BENCHMARK RESULT: FAIL
```

The complete run may take several minutes. Use ten elapsed minutes as the hard stop. If there is still no explicit verdict, photograph the screen and reset the machine. A reset is an expected recovery action for this candidate harness; do not wait indefinitely.

## 5. What to Observe

Please report all of the following, including apparently uneventful behavior:

- elapsed time from boot start to the final verdict or reset;
- exact final text, if any;
- whether the screen blanked, changed size, flickered, froze, or showed corrupt graphics;
- whether any unexpected sound was audible;
- whether a Guru Meditation or spontaneous reset occurred;
- whether the drive LED remained active at the ten-minute limit;
- whether the CLI and normal display returned after PASS or controlled FAIL;
- whether keyboard and mouse behavior appeared normal after return;
- whether a second reset was required to recover the machine.

Do not interpret a blank or flickering screen during the measured interval as a failure by itself. The benchmark deliberately switches display, Copper, sprite, audio, and blitter DMA combinations. Persistent corruption after the final text screen returns is a failure.

## 6. Result File Meanings

`RESULT.TXT` is written to the root of the test disk. Its state distinguishes four outcomes:

| Disk state | Meaning |
| --- | --- |
| No `RESULT.TXT` | The program did not reach its writable-media preflight, or the medium did not retain writes. |
| Last line `result=running` | The program started and the medium was writable, but the exclusive suite did not complete before reset or power-off. |
| Last line `result=fail` | The program recovered through its controlled cleanup path and wrote diagnostics. |
| Last line `result=pass` | All 126 cases, checksums, stack checks, and state-restoration checks completed. The report must still pass the host validator. |

For `running`, FAIL, a Guru, or a corrupt display, return the image unchanged. Partial and failed images are valuable diagnostics; do not replace them with another run.

## 7. Tester Result Form

Append this information to the pre-run record:

```text
Start time:
End time or reset time:
Elapsed time:
Final verdict: PASS / FAIL / no verdict / Guru / reset
Final screen photographed: yes / no
Expected display changes observed: yes / no
Unexpected visual behavior:
Unexpected sound:
Drive LED inactive before media removal: yes / no
CLI/display restored: yes / no / not applicable
Keyboard and mouse usable after return: yes / no / not tested
Recovery action, if any:
Returned ADF filename:
Returned ADF SHA-256:
Additional notes:
```

Return the modified ADF for every run, the completed forms, and the screen photographs. Compressing the files for transport is fine, but do not edit the ADF contents.

## 8. Extracting and Validating a Returned Run

On the development Mac, preserve the returned ADF and extract its report to a separate file:

```sh
mkdir -p build/reports
xdftool friend-run-01-returned.adf read RESULT.TXT \
  build/reports/friend-run-01.txt
tail -n 5 build/reports/friend-run-01.txt
```

Only a complete PASS report is accepted by the strict validator:

```sh
./scripts/validate-exclusive-graphics-benchmark-report.sh \
  build/reports/friend-run-01.txt
```

Before using its timings, also verify:

- `environment=physical_a1200_pal_candidate`;
- `timing_authority=real_hardware_candidate`;
- `detected_stock_constraints=pass`;
- `power_supply_hz=50`;
- `available_fast_bytes_before_setup=0`;
- `benchmark_instruction_cache=active`;
- `raster_timeout_count=0`;
- `interrupt_restore=pass` and `dma_restore=pass`;
- the tester's machine declaration agrees with the report.

`real_hardware_candidate` is intentionally not final authority. Backend selection requires repeated, validated distributions from a declared stock PAL A1200 and closure of the remaining workload limitations described below.

## 9. Scope and Limitations

This ADF measures six raw Chip-RAM kernels and two real 68020 C2P4 kernels over three viewport sizes, two source layouts, and seven additive DMA profiles. It checks 126 cases against deterministic checksums. It uses a guarded dedicated stack, direct raster polling, real four-channel muted Paula DMA, and fair/hog blitter-copy controls. It restores the caller's custom-chip and instruction-cache state before writing the final verdict.

It does not yet provide representative sprite payloads, sustained blitter work for the whole longest conversion, a genuine CPU/blitter C2P merge, safe frame publication, or the future MAGI-80 Level-3 runtime interrupt path. A PASS therefore validates this matrix and its restoration behavior; it does not alone select the final graphics architecture or prove the 25 Hz frame budget.

The complete physical-test contract and interpretation rules are maintained in [Exclusive Graphics Benchmark Plan](./graphics-exclusive-benchmark.md).
