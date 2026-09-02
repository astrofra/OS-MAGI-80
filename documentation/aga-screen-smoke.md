# MAGI-80 Hosted AGA Screen Smoke Test

**Status:** Passing under FS-UAE 3.2.35 with the stock PAL A1200 profile and Kickstart 3.0/39.106 on 2026-09-02

**Target:** Stock Amiga 1200, 68EC020, AGA, 2 MiB Chip RAM, no Fast RAM or FPU

## 1. Purpose and Boundary

This smoke test proves that the selected C99/libnix ABI can use the documented AmigaOS 3.0 graphics stack to create the display shape required by MAGI-80 and feed it with the portable reference converter:

- a 256 × 256 PAL low-resolution screen;
- eight displayable bitplanes in Chip RAM;
- two four-bit playfields;
- foreground transparency over a background playfield;
- separate palette bases for the two playfields;
- clean return to the AmigaOS display.

It deliberately remains a hosted, cooperative test. Intuition and `graphics.library` create and own the display. `SA_Exclusive` prevents an incompatible screen from sharing the display, but task scheduling remains active: the test does not call `Forbid()`, install a custom Copper list, write custom-chip registers, or take over interrupts. Those operations belong to the later exclusive-runtime smoke test.

## 2. Provisional Display Mapping

| MAGI-80 layer | AGA playfield | Hardware bitplanes | Priority | Color base |
|---|---|---|---|---:|
| `FRONT` | PF1 | BPL1, BPL3, BPL5, BPL7 | In front | 0 |
| `BACK` | PF2 | BPL2, BPL4, BPL6, BPL8 | Behind | 16 |

The screen uses `PAL_MONITOR_ID | LORESDPF_KEY`. PF1 index 0 is transparent; PF1 indices 1–15 select color registers 1–15. PF2 index 0 reveals global backdrop color 0; PF2 indices 1–15 use registers 17–31 through the base-16 offset. This realizes the required 16 background colors plus 15 opaque foreground colors.

The smoke test uses Intuition's non-interleaved eight-plane bitmap. It allocates a 65,536-byte combined chunky framebuffer, fills a deterministic two-layer pattern, and passes the eight real Chip-RAM plane pointers to the portable C99 converter. The converter writes foreground bits to bitmap planes 0, 2, 4, and 6 and background bits to planes 1, 3, 5, and 7. The independently retained pixel encoder is used only to calculate expected `ReadPixel()` results.

The 32-entry test palette is loaded through `LoadRGB32()`. Each virtual four-bit component is expanded by nibble replication, then read back with `GetRGB32()` and checked at four-bit precision. `VideoControl()` sets and queries the PF1/PF2 bases and enables full palette loading.

## 3. Standalone-Boot Chipset Activation

The minimal test ADF does not execute the Workbench `SetPatch` command. On the first run, `GfxBase->ChipRevBits0` therefore did not yet expose the AGA feature bits even though FS-UAE was configured as an A1200.

The test now follows the documented standalone-program path: if the Alice/Lisa bits are absent, it calls `SetChipRev(SETCHIPREV_BEST)` once and verifies the returned capabilities before requesting the AGA-depth display. A normal Workbench launch whose startup has already enabled AGA skips this call.

## 4. Automated Assertions

The executable fails with a named stage unless all of the following hold:

1. `graphics.library` and `intuition.library` version 39 open successfully.
2. AGA Alice and Lisa capabilities are active.
3. the display database exposes an available PAL dual-playfield mode.
4. Intuition opens an exact 256 × 256, eight-plane screen.
5. the ViewPort is the requested mode and has two `RasInfo` playfields.
6. all eight bitmap planes are non-null, displayable, and in Chip RAM.
7. opening the screen consumes at least the expected 65,536 plane bytes.
8. PF1/PF2 palette bases read back as 0 and 16.
9. all 32 palette entries round-trip at the MAGI-80 four-bit component precision.
10. a 65,536-byte chunky framebuffer is allocated and the reference converter accepts the real plane layout and row stride.
11. converted background and foreground pixels preserve the other playfield's bits at five probe positions.
12. the pattern remains displayed for 50 PAL frames.
13. the chunky buffer is freed and a second open/close cycle has no incremental Chip-RAM loss.

The repeated cycle matters because the first Intuition screen initializes persistent system display caches. Treating the entire difference between the cold pre-screen and post-screen `AvailMem()` readings as a leak produced a false failure. Comparing a second identical cycle separates those one-time OS allocations from per-screen leakage.

## 5. Build and Run

Run the dedicated test with:

```sh
gmake aga-screen-smoke
```

It is also part of the normal regression command:

```sh
gmake check
```

The generated files are kept below `build/`:

```text
build/smoke/aga-screen/program
build/smoke/aga-screen/program.map
build/reports/aga-screen-size.txt
build/reports/aga-screen-symbols.txt
build/reports/aga-screen-disassembly.txt
```

The validated executable is a 7,812-byte Hunk file containing 5,160 bytes of text, 208 bytes of data, and 824 bytes of BSS. It introduces no `stdio`, C allocator, or floating-point dependency; the framebuffer is explicitly managed through Exec `AllocMem()` and `FreeMem()`.

The generic FS-UAE runner temporarily stages this executable, compares its redirected AmigaDOS output with `tests/smoke/aga-screen/expected.txt`, and restores the previously staged MAGI-80 executable even on failure or interruption.

## 6. What This Does Not Prove

This result does not yet validate:

- an optimized 68020 converter or its frame cost;
- double buffering and safe VBlank plane-pointer swaps;
- direct AGA register values, fetch mode, modulo, or a MAGI-80 Copper list;
- hosted-to-exclusive takeover and restoration;
- visual output against a captured golden image;
- Kickstart 3.1 behavior;
- timing, Chip-RAM contention, or restoration on a physical A1200.

The converter contract and native golden vectors are documented in [Reference Chunky-to-Planar Converter](./c2p-reference.md). The next graphics step is to establish an optimized path and frame-cost measurements while preserving byte identity with this reference, before attempting double buffering or direct-hardware takeover.

## 7. System References

- [Intuition `OpenScreen()` and screen tags](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_3._guide/node03D3.html)
- [`graphics.library/VideoControl()`](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_3._guide/node0338.html)
- [`graphics.library/LoadRGB32()`](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_3._guide/node02FB.html)
- [Display memory and dual `RasInfo` structures](https://amigadev.elowar.com/read/ADCD_2.1/Libraries_Manual_guide/node032B.html)
- [AGA dual-playfield priority](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node007B.html)
