# MAGI-80 Chunky Layout and C2P Benchmark

**Status:** Reference-C baseline implemented and validated under FS-UAE 3.2.35 with Kickstart 3.0/39.106; no runtime layout has been selected

**Decision authority:** A physical stock PAL A1200 with 68EC020, 2 MiB Chip RAM, no Fast RAM, no FPU, display DMA active, and eventually four-channel audio active

## 1. Question

MAGI-80 exposes two logical 16-color layers, but that does not determine their in-memory representation. The benchmark compares the cost of constructing and modifying candidate source layouts as well as converting them to the same eight AGA dual-playfield planes.

The layout MUST remain private to the runtime until this decision is frozen. Cartridge assets have an independent serialized representation and MAY be expanded or preconverted when loaded.

## 2. Candidate Layouts

| Identifier | Runtime representation | Source bytes | Single-layer pixel write | Minimum full C2P traffic |
|---|---|---:|---|---:|
| `fb8` | One byte `0xFB` per screen coordinate | 65,536 | Read, mask, write | 131,072 bytes |
| `packed4_x2` | Separate packed FRONT and BACK, two spatial pixels per byte | 65,536 | Nibble read-modify-write | 131,072 bytes |
| `byte4_x2` | Separate FRONT and BACK bytes, low nibble significant | 131,072 | Direct byte store | 196,608 bytes |
| `front_byte4_back_packed4` | Byte FRONT and packed BACK | 98,304 | Direct FRONT byte store | 163,840 bytes |

The traffic column is only the source read plus the 65,536-byte planar write. It excludes instruction fetches, intermediate buffers, extra merge passes, display DMA, audio DMA, cache effects, and allocator activity.

Two planar display buffers add 131,072 bytes independently of the selected source. The corresponding baseline framebuffer totals are therefore 192 KiB, 192 KiB, 256 KiB, and 224 KiB before code, assets, C2P scratch space, audio, and editor state.

## 3. Correctness Contract

All four layouts describe the same logical pixels and MUST produce the bitplane order defined in [Reference Chunky-to-Planar Converter](./c2p-reference.md). The native suite applies the same hand-calculated eight-pixel vector and the same complete 256 × 256 vector to every layout. Byte-per-pixel inputs contain deliberate noise in their unused high nibbles to prove that only the low four bits are significant.

The Amiga benchmark also requires every candidate to produce the same checksum over all 65,536 visible planar bytes. This is a regression guard, not a substitute for the native golden vectors.

## 4. Reference Benchmark Protocol

`tests/benchmark/c2p-layouts/main.c` is cross-compiled for 68020 soft-float with GCC `-O2` and `libnix`. It performs the following work for each layout:

1. allocate the source buffers explicitly in Chip RAM;
2. construct the same complete 256 × 256 two-layer pattern using a layout-specific sequential loop;
3. perform 16,384 deterministic pseudo-random writes to FRONT using the layout's natural pixel update;
4. convert the whole image with the corresponding allocation-free C99 reference converter;
5. write directly to the eight planes of an active, Intuition-managed PAL AGA screen;
6. checksum the visible plane bytes and require cross-layout equality;
7. free the source before testing the next layout.

Each phase is bracketed with `timer.device/ReadEClock()`. A `WaitTOF()` precedes each timed interval, but is not included in it. Multitasking remains enabled and bitplane DMA remains active. The report records raw E-Clock ticks, the returned clock rate, and the minimum cost of two adjacent timer reads.

The current run uses one sample. That is adequate to validate the protocol but not to choose an implementation. Future optimized candidates need warm-up, repeated samples, minimum/median/worst values, and a controlled exclusive-runtime mode.

Run the matrix with:

```sh
gmake c2p-benchmark-fs-uae
```

The machine-readable result is copied to `build/reports/c2p-layouts-fs-uae.txt` and validated by `scripts/validate-c2p-benchmark-report.sh`. The executable, map, sorted symbols, and disassembly remain under `build/benchmark/c2p-layouts/` and `build/reports/`.

## 5. Initial FS-UAE Baseline

This 2026-09-02 run used a reported E-Clock rate of 709,379 Hz and a 40-tick timer-pair overhead. The millisecond columns are derived from the raw ticks and are shown only for readability.

| Layout | Build ticks | FRONT pset ticks | C2P ticks | Phase sum ticks | Phase sum |
|---|---:|---:|---:|---:|---:|
| `fb8` | 231,557 | 87,711 | 1,156,880 | 1,476,148 | 2,080.90 ms |
| `packed4_x2` | 325,295 | 101,089 | 1,378,209 | 1,804,593 | 2,543.91 ms |
| `byte4_x2` | 234,733 | 54,787 | 1,199,874 | 1,489,394 | 2,099.57 ms |
| `front_byte4_back_packed4` | 302,065 | 54,013 | 1,288,553 | 1,644,631 | 2,318.41 ms |

All four checksums were `916915588`.

The byte FRONT candidates make the expected direct-store advantage visible: their FRONT update phase is roughly one third cheaper than `fb8` in this workload. Conversely, `fb8` currently has the cheapest reference conversion and the lowest phase sum. `byte4_x2` is close enough that a more pixel-heavy workload or a specialized four-plane converter could reverse the order.

No absolute time is acceptable: even the fastest scalar reference C2P took about 1.63 seconds, versus a 40 ms budget at 25 updates per second. The result proves that the benchmark distinguishes representation costs and that the layouts are equivalent; it does not rank optimized designs.

FS-UAE is a functional and protocol-validation environment. Its E-Clock results MUST NOT be used as release performance claims or as the final layout decision.

## 6. Optimized Candidate Matrix

The next benchmark tranche MUST add:

- a CPU-only 68020 assembly transpose processing at least 16 and preferably 32 pixels per iteration;
- an inner loop designed around the 68EC020's 256-byte instruction cache;
- a `CPU3BLIT1`-style hybrid in which the CPU performs early merge stages and the blitter performs the final merge;
- full eight-plane conversion, FRONT-only four-plane conversion, aligned dirty rectangles, and no-change paths;
- direct-planar blitter baselines for clear, opaque sprite, masked sprite, tile row, and scroll operations;
- active-display and blanked-display measurements, with blitter priority recorded;
- integrated frame measurements that include rendering, conversion, pointer swap, and eventually Paula playback.

The blitter is asynchronous but shares Chip RAM bandwidth with the CPU and display DMA. A hybrid therefore succeeds only if saved CPU merge work and useful overlap outweigh its extra intermediate traffic. Commodore documents the DMA arbitration and `BLITHOG` behavior in [Blitter Operations and System DMA](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node012B.html). The classic `CPU3BLIT1` merge split is explained in the [Kalms C2P tutorial](https://amycoders.org/sources/c2ptut.html).

The 68EC020 has an instruction cache but no data cache; compact hot loops matter while every source and destination access still reaches memory. See the [Motorola M68020 User's Manual](https://www.ele.uva.es/~jesman/BigSeti/ftp/Microprocesadores/Motorola/68020um.pdf). The AGA blitter remains word-oriented; AGA's wider bitplane fetch modes do not turn it into a general chunky-pixel engine. See the [Commodore AA Chip Set Functional Specification](https://shanson.com/spencer/Amiga-AA-Chipset.pdf).

## 7. Decision Gate

The source layout may be frozen only after the optimized matrix runs on real hardware. The decision record must include:

- total frame cost rather than C2P time alone;
- workloads dominated by `pset`, sprites, maps, fills, and scrolling;
- full-frame and one-layer-dirty cases;
- memory and scratch-space cost;
- CPU time available for generated MAGI Lua code;
- audio stability and missed-frame behavior;
- whether a planar-native fast path can avoid conversion for high-level primitives without changing language semantics.

Until then, `fb8` is the implementation baseline and correctness oracle, not the selected MAGI-80 ABI.
