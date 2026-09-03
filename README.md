# OS-MAGI-80

Fantasy OS for the Amiga 1200

The first hosted AmigaOS bootstrap can be built and tested from macOS with:

```sh
gmake check
```

This runs the native legacy eight-plane and new four-plane chunky-to-planar golden vectors, three-layer graphics oracle, inverse AGA decoder differential, and graphics-report schema tests; it also compiles and inspects the Hunk executables, runs the hosted bootstrap through `vamos` and FS-UAE, then executes the 256 × 256 AGA screen regression under FS-UAE.

Run only the portable three-layer graphics oracle natively with:

```sh
gmake graphics-reference-test
```

Run the inverse dual-playfield decoder and compositor → C2P → decoder differential with:

```sh
gmake aga-reference-test
```

Validate the common Phase 0 graphics benchmark report contract with:

```sh
gmake graphics-report-test
```

Run only the historical two-layer/eight-plane C99 converter tests natively on macOS with:

```sh
gmake c2p-test
```

Run the historical four-layout C2P measurement protocol under FS-UAE with:

```sh
gmake c2p-benchmark-fs-uae
```

Its scalar-C timings validate the benchmark infrastructure; the runtime layout remains open until optimized candidates are measured on a real stock A1200.

Run the three-profile, single-layer C2P4 correctness suite natively with:

```sh
gmake c2p4-test
```

Build and execute the C99 pair-LUT, 68020 pair-LUT/mask32, and staged blitter-publication matrix under FS-UAE with:

```sh
gmake c2p4-benchmark-fs-uae
```

This validates all 24 optimized-path cases, conservative traffic accounting, and exact canonical output. FS-UAE timings do not select a runtime layout or backend; an exclusive-runtime harness, physical Chip-RAM calibration, genuine CPU/blitter conversion, and stock-A1200 measurements remain open.

Run the hosted 256×256 AGA dual-playfield smoke test separately with:

```sh
gmake aga-screen-smoke
```

The target is currently locked to the `libnix` Kickstart 2+ startup/runtime with
`-mcrt=nix20`. Reproduce the `newlib`/`libnix`/`clib2` allocation and filesystem
comparison with:

```sh
gmake runtime-compare
```

Launch it interactively under the configured Workbench 3.0 FS-UAE profile with:

```sh
gmake run
```

See the [macOS development toolchain guide](documentation/macos-development-toolchain.md) for local ROM/HDF configuration and validated versions, the [three-layer graphics reference](documentation/graphics-reference-compositor.md) for canonical composition semantics, the [AGA reference decoder](documentation/aga-reference-decoder.md) for inverse playfield validation, the [graphics benchmark report format](documentation/graphics-benchmark-report-format.md) for comparable Phase 0 evidence, the [four-plane C2P benchmark](documentation/c2p4-benchmark.md) for the current viewport converters, the [exclusive graphics benchmark plan](documentation/graphics-exclusive-benchmark.md) for authoritative hardware timing and takeover diagnostics, the [legacy C2P reference note](documentation/c2p-reference.md) and [layout benchmark](documentation/c2p-layout-benchmark.md) for the historical two-layer experiments, and the [AGA screen smoke-test note](documentation/aga-screen-smoke.md) for the provisional playfield mapping and current validation boundary.
