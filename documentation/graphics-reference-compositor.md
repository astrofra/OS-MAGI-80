# MAGI-80 Three-Layer Graphics Reference Compositor

**Status:** Initial portable C99 oracle implemented; native golden and sanitizer tests pass on macOS

**Role:** Authoritative logical composition for `PLANAR`, positioned `PIXEL`, and ordered `OBJECTS` before any AGA-specific representation or optimization is selected

## 1. Purpose

The reference compositor defines what a MAGI-80 frame means without depending on AGA bitplanes, C2P layout, blitter commands, Copper scheduling, or physical sprite allocation. Future native-planar, chunky-conversion, hardware-sprite, and fallback paths must reproduce its visible output.

It deliberately optimizes for clarity and testability rather than speed. It is not a candidate runtime renderer.

## 2. Canonical Pixel Output

The oracle writes a complete 256 × 256 byte-per-pixel image:

| Output value | Meaning |
| ---: | --- |
| `0..15` | `PLANAR` palette index `0..15` |
| `16..30` | Opaque `PIXEL` or `OBJECTS` palette index `1..15` |

Overlay/object index 0 is transparent. Its canonical output mapping is therefore `15 + source_index` for non-zero indices. The output represents palette identities, not RGB values.

The logical composition order is:

```text
OBJECTS
   over
PIXEL viewport
   over
PLANAR base
```

## 3. Reference Inputs

The public declarations are in `src/graphics/reference_compositor.h`; the implementation is in `src/graphics/reference_compositor.c`.

All input surfaces use one byte per logical pixel with only the low nibble significant. This intentionally simple oracle representation is independent of:

- packed versus byte-per-pixel chunky viewport storage;
- native AGA planar storage and interleaving;
- cached blitter tile/object formats;
- hardware-sprite control/data words.

A view selects a validated source rectangle and places it at a signed display position. This supports source backing margins, positioned viewports, and clipping at all display edges without exposing a runtime buffer layout.

Objects select a rectangle from an image surface and use signed world coordinates. The scene's object-camera offset is subtracted before clipping.

## 4. Object Ordering and Backend Equivalence

Objects always compose above the pixel and planar layers in the baseline contract.

Within `OBJECTS`:

1. lower numeric priority is drawn first;
2. higher numeric priority is on top;
3. for equal priority, a later object-list entry is on top;
4. index 0 never replaces the pixel below it.

Each object carries an internal backend hint: automatic, hardware sprite, or planar fallback. The oracle validates the hint but ignores it when producing pixels. Changing only this hint must leave every visible output byte unchanged. This gives hardware and fallback implementations one shared expected image.

The reference implementation accepts at most 256 objects to keep malformed tests bounded. This is an oracle safety ceiling, not the frozen cartridge object limit.

## 5. Validation Contract

Before modifying output, the compositor validates:

- scene and output pointers;
- the 256-byte minimum output stride;
- object-list pointer and reference ceiling;
- enabled-view and visible-object surface pointers;
- non-zero dimensions no larger than the logical display;
- source and output stride arithmetic;
- source rectangles against their surfaces;
- object backend hints.

On success, all 65,536 visible bytes are overwritten and output row padding remains untouched. On validation failure, output remains untouched. Inputs and output must not overlap.

## 6. Native Golden Suite

Run:

```sh
gmake graphics-reference-test
```

The suite covers:

- the three-layer composition order and canonical palette mapping;
- transparent pixel and object pixels;
- higher-priority objects and equal-priority list-order tie-breaking;
- byte-per-pixel inputs containing deliberate noise in the ignored high nibble;
- identical output after swapping hardware-sprite and planar-fallback hints;
- non-zero source origins and padded source strides;
- negative screen placement and right-edge clipping;
- object-camera translation and object clipping;
- preserved output row padding;
- invalid pointers, dimensions, strides, regions, object counts, and backend hints;
- no output modification when validation fails.

The expected report is stored in `tests/host/graphics-reference/expected.txt`; the generated report is stored in `build/reports/graphics-reference-host.txt`.

## 7. Relationship to the Next Benchmarks

The next four-plane C2P and native-planar benchmarks should use this oracle as follows:

1. construct one deterministic logical scene;
2. compose its canonical expected image with this implementation;
3. render the same scene through the candidate backend;
4. decode the resulting AGA playfields and adapt any hardware-sprite output into canonical palette identities;
5. require byte equality or an identical canonical checksum before considering timing results.

The first performance tranche remains:

- packed and byte-per-pixel 4-bit sources;
- 160 × 128, 192 × 160, and 256 × 256 viewports;
- CPU-only, blitter-assisted, and CPU/blitter-hybrid four-plane conversion;
- active display DMA, followed later by combined sprite and Paula DMA.

The portable [AGA Dual-Playfield Reference Decoder](./aga-reference-decoder.md) now implements the inverse PF1/PF2 mapping and proves a full-frame compositor → C2P → decoder round trip. The existing [Reference Chunky-to-Planar Converter](./c2p-reference.md) remains the byte-level plane oracle until the new single-layer four-plane converters are implemented. Hardware-sprite output still needs a separate canonical adapter when the object benchmark tranche begins.

Every new benchmark must emit the validated [Graphics Benchmark Report Format](./graphics-benchmark-report-format.md), including matching oracle/candidate checksums before its timing data can be considered.

## 8. Deliberate First-Version Limits

This initial compositor does not yet define:

- alternative object priority bands between playfields;
- wrapping or scroll-rebase behavior;
- dirty rectangles or damage restoration;
- palette RGB programming;
- object allocation, multiplex limits, or fallback selection policy;
- collision, scaling, rotation, blending, or partial alpha;
- hardware timing or memory bandwidth.

Those features must be added only with explicit logical semantics and golden cases. UAE can validate their integration protocol; a stock A1200 remains the performance authority.
