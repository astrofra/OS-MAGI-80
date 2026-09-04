# MIGA-80 AGA Dual-Playfield Reference Decoder

**Status:** Portable C99 decoder implemented; native golden, differential, and sanitizer tests pass on macOS

**Role:** Translate an eight-bitplane AGA dual-playfield frame back into the same canonical palette identities as the three-layer graphics compositor

## 1. Purpose

An optimized graphics backend is not correct merely because its bitplanes look plausible. MIGA-80 needs a deterministic way to compare those bitplanes with the logical scene that produced them.

`src/graphics/aga_reference_decoder.c` provides that inverse mapping. It decodes the two four-plane AGA playfields into a 256 × 256 byte-per-pixel image that can be compared directly with `miga80_graphics_reference_compose()` output. It is a correctness tool, not runtime display code.

## 2. Plane and Color Mapping

The decoder uses the provisional mapping already exercised by the hosted AGA smoke test:

| Source index | AGA plane | Playfield | Logical bit |
| ---: | --- | --- | ---: |
| 0 | BPL1 | `PIXEL` / PF1 | 0 |
| 1 | BPL2 | `PLANAR` / PF2 | 0 |
| 2 | BPL3 | `PIXEL` / PF1 | 1 |
| 3 | BPL4 | `PLANAR` / PF2 | 1 |
| 4 | BPL5 | `PIXEL` / PF1 | 2 |
| 5 | BPL6 | `PLANAR` / PF2 | 2 |
| 6 | BPL7 | `PIXEL` / PF1 | 3 |
| 7 | BPL8 | `PLANAR` / PF2 | 3 |

Bit 7 is the leftmost pixel of each plane byte. A non-zero PF1 value is opaque and maps to canonical identity `15 + value`, or 16–30. PF1 value 0 is transparent and reveals PF2 identity 0–15.

This produces palette identities rather than RGB values. Palette-register programming is tested separately.

## 3. API and Validation

The public API is declared in `src/graphics/aga_reference_decoder.h`:

```c
enum Miga80AgaReferenceStatus miga80_aga_reference_decode_dual_playfield(
    const uint8_t *planes[8],
    size_t plane_stride,
    uint8_t *output,
    size_t output_stride);
```

The first version deliberately has a fixed 256 × 256 visible extent. Each plane row contains 32 visible bytes; larger source and output strides are accepted.

Before writing any output, the function validates:

- the plane-array, all eight plane pointers, and the output pointer;
- the 32-byte minimum plane stride and 256-byte minimum output stride;
- all stride and address-span arithmetic;
- absence of overlap between output storage and every source plane.

On success it overwrites all 65,536 visible output bytes and preserves output row padding. On failure it leaves output untouched. Source planes are read-only and their padding is never modified.

## 4. Differential Test

Run:

```sh
gmake aga-reference-test
```

The native suite verifies:

- an independently calculated eight-pixel plane vector;
- transparent PF1 and canonical PF1/PF2 palette mapping;
- a deterministic full-frame round trip from the logical compositor through the existing byte-per-pixel C2P and back through this decoder;
- padded source and output strides with untouched sentinels;
- null pointers, undersized strides, overflowing stride arithmetic, and forbidden source/output overlap;
- no output modification after validation failure.

The expected report is `tests/host/aga-reference-decoder/expected.txt`; the generated report is `build/reports/aga-reference-decoder-host.txt`.

The full-frame differential is the first reusable backend proof:

```text
logical PLANAR + PIXEL scene
        |                         |
        | reference compositor    | candidate C2P
        v                         v
canonical expected          eight AGA bitplanes
        |                         |
        |                         | reference decoder
        v                         v
        exact 65,536-byte comparison
```

## 5. Boundary of This Version

The decoder reconstructs playfield composition only. Hardware sprites are separate DMA objects and do not exist in the eight bitplanes, so the decoder cannot yet prove the complete `OBJECTS` layer.

The object benchmark tranche must add a canonical sprite-output adapter. It may model the scheduled sprite streams or capture final pixels, but it must preserve the same object palette, transparency, priority, clipping, and fallback semantics as the compositor. Planar fallback objects can already be checked when they are rendered into PF1 before decoding.

The decoder also does not validate Copper timing, fetch windows, modulo programming, display publication, RGB palette precision, or real-hardware performance. Those remain UAE integration and stock-A1200 gates.
