# MAGI-80 Reference Chunky-to-Planar Converter

**Status:** Portable C99 implementation and native golden-vector suite passing on macOS; the same source is validated against an Intuition-managed AGA screen under FS-UAE 3.2.35 and Kickstart 3.0/39.106

**Role:** Correctness oracle for candidate runtime layouts and future optimized 68020 implementations

## 1. What a Golden Vector Is

A golden vector is a small, fixed input paired with its independently calculated expected output. The test runs the converter and requires an exact byte-for-byte match. “Executable natively on macOS” means that Apple Clang compiles the portable converter and its test as a normal macOS process. It does not mean that an Amiga executable runs directly on macOS.

This gives a fast development loop for pure algorithms. It catches bit ordering, playfield assignment, row-stride, and overwrite errors without booting an emulator. It cannot prove that an Amiga display interprets those bytes correctly, so the same converter is also cross-compiled for the 68020 and exercised on a real eight-plane screen under FS-UAE.

## 2. Conversion Contract

The public reference routines are declared in `src/graphics/c2p_reference.h`. The `fb8` baseline is implemented in `src/graphics/c2p_reference.c`; the experimental layouts are isolated in `src/graphics/c2p_reference_layouts.c` so they do not increase the normal AGA smoke-test binary. They:

- accept the combined `fb8`, split packed, split byte, and asymmetric source layouts;
- interpret an `fb8` pixel as `0xFB`, where `F` is the four-bit `FRONT` value and `B` is the four-bit `BACK` value;
- place the even x pixel in the high nibble and the odd x pixel in the low nibble of packed sources;
- ignore the high nibble of byte-per-pixel sources;
- writes eight separate, non-interleaved planar destinations;
- uses explicit source and destination row strides;
- requires a non-zero width divisible by eight and a non-zero height;
- performs no allocation and calls no C or AmigaOS library function;
- assumes that source and destination storage do not overlap.

Within every planar byte, bit 7 represents the leftmost pixel and bit 0 the rightmost pixel.

| Destination index | AGA plane | Playfield | Logical bit |
|---:|---|---|---:|
| 0 | BPL1 | `FRONT` / PF1 | 0 |
| 1 | BPL2 | `BACK` / PF2 | 0 |
| 2 | BPL3 | `FRONT` / PF1 | 1 |
| 3 | BPL4 | `BACK` / PF2 | 1 |
| 4 | BPL5 | `FRONT` / PF1 | 2 |
| 5 | BPL6 | `BACK` / PF2 | 2 |
| 6 | BPL7 | `FRONT` / PF1 | 3 |
| 7 | BPL8 | `BACK` / PF2 | 3 |

## 3. Native Golden Vectors

The smallest vector describes these eight logical pixels in `fb8` notation:

```text
F0 81 42 24 18 0F 5A A5
```

Every source layout must produce these expected bytes in BPL1-to-BPL8 order:

```text
8A 45 91 26 A2 15 C1 0E
```

The native suite also checks:

- a two-row, 16-pixel vector with padded source and destination strides, including untouched sentinel bytes;
- the same hand-calculated vector through all four source layouts;
- a complete 256 × 256 image filled logically with `FRONT=A`, `BACK=5`, checking every output byte from every layout in all eight planes;
- null pointers, zero or non-byte-aligned dimensions, and undersized strides.

Run it with:

```sh
gmake c2p-test
```

The Mach-O test result is compared with `tests/host/c2p-reference/expected.txt` and recorded in `build/reports/c2p-reference-host.txt`.

## 4. Amiga Integration Check

`gmake aga-screen-smoke` cross-compiles the exact same `c2p_reference.c` with `m68k-amigaos-gcc`. The Amiga test:

1. opens a 256 × 256 × 8 PAL dual-playfield screen;
2. allocates a 65,536-byte combined chunky framebuffer with `AllocMem()`;
3. fills it with a deterministic two-layer pattern;
4. passes the eight actual displayable Chip-RAM plane pointers and bitmap row stride to the reference converter;
5. reads representative pixels back through `graphics.library`;
6. frees the framebuffer and verifies stable screen teardown.

This closes the gap between a host-only byte test and the OS-managed AGA bitmap layout. The full regression command runs both layers:

```sh
gmake check
```

The separate layout and timing experiment is documented in [Chunky Layout and C2P Benchmark](./c2p-layout-benchmark.md) and runs with `gmake c2p-benchmark-fs-uae`.

## 5. Deliberate Limitations

The reference converters favor clarity and bit-exact behavior over speed. They perform per-pixel and per-bit loops and are not intended to meet the final frame budget on a stock 68EC020.

The next implementation may use 32-bit transposes, lookup tables, assembly, or a hybrid C/assembly path. Any optimized converter must remain byte-identical to this reference across deterministic vectors, randomized frames, dirty rectangles, and non-trivial strides. Performance acceptance still requires measurement on a physical stock A1200 with bitplane DMA active.

This test also does not yet validate double buffering, VBlank-safe pointer swaps, a custom Copper list, exclusive takeover, or restoration after failure.
