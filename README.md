# OS-MAGI-80

Fantasy OS for the Amiga 1200

The first hosted AmigaOS bootstrap can be built and tested from macOS with:

```sh
gmake check
```

This compiles and inspects the Hunk executable, runs it through `vamos`, then executes it from the `MAGI80:` host-directory mount in FS-UAE.

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

See the [macOS development toolchain guide](documentation/macos-development-toolchain.md) for local ROM/HDF configuration and validated versions.
