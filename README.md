# OS-MAGI-80

Fantasy OS for the Amiga 1200

The first hosted AmigaOS bootstrap can be built and tested from macOS with:

```sh
gmake check
```

This runs the native chunky-to-planar golden vectors, compiles and inspects the Hunk executables, runs the hosted bootstrap through `vamos` and FS-UAE, then executes the 256 × 256 AGA screen regression under FS-UAE.

Run only the portable C99 converter tests natively on macOS with:

```sh
gmake c2p-test
```

Run the four-layout C2P measurement protocol under FS-UAE with:

```sh
gmake c2p-benchmark-fs-uae
```

Its scalar-C timings validate the benchmark infrastructure; the runtime layout remains open until optimized candidates are measured on a real stock A1200.

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

See the [macOS development toolchain guide](documentation/macos-development-toolchain.md) for local ROM/HDF configuration and validated versions, the [C2P reference note](documentation/c2p-reference.md) for the golden-vector contract, the [layout benchmark](documentation/c2p-layout-benchmark.md) for the open representation decision, and the [AGA screen smoke-test note](documentation/aga-screen-smoke.md) for the provisional playfield mapping and current validation boundary.
