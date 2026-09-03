# MAGI-80 macOS Development Toolchain Plan

**Document status:** Cross-toolchain, initial `libnix` ABI, native C2P goldens, hosted bootstrap, AGA dual-playfield conversion, and the 24-case pair-LUT/mask32 C2P4 protocol validated through native execution, `vamos`, and FS-UAE/Kickstart 3.0; exclusive fine timing, genuine hybrid C2P, Kickstart 3.1, and real-hardware validation remain pending

**Host:** Apple Silicon Mac running macOS 14.1

**Target:** Stock PAL Amiga 1200, 68EC020, AGA, 2 MiB Chip RAM, no Fast RAM or FPU

**Related document:** [MAGI-80 Specification and Roadmap](./MAGI-80-specification-and-roadmap.md)

**Graphics test note:** [Hosted AGA Screen Smoke Test](./aga-screen-smoke.md)

**C2P correctness note:** [Reference Chunky-to-Planar Converter](./c2p-reference.md)

**C2P benchmark note:** [Chunky Layout and C2P Benchmark](./c2p-layout-benchmark.md)

**Current four-plane benchmark:** [Four-Plane C2P Reference and Benchmark](./c2p4-benchmark.md)

## 1. Purpose

This document defines the development environment required to build, inspect, package, run, and test MAGI-80 from macOS.

The toolchain must support:

- C99 cross-compilation to AmigaOS Hunk executables;
- small, explicitly selected 68020 assembly modules;
- AmigaOS 3.0 and 3.1 headers and libraries;
- deliberate reuse of the documented AmigaOS libraries, devices, resources, and filesystem APIs;
- inspection of code size, symbols, relocations, and generated 68k instructions;
- creation and inspection of OFS and FFS ADF images;
- rapid execution of AmigaOS CLI programs without starting a complete emulator;
- accurate A1200 integration tests in FS-UAE;
- both hard-disk/directory and bootable-floppy development workflows;
- reproducible builds without committing proprietary ROM or Workbench files.

This is a host-development setup. It does not alter the MAGI-80 architectural decision to use AmigaOS services while editing and to take exclusive control only while a game is running.

## 2. Recommended Tool Stack

| Layer | Selected tool | Purpose |
|---|---|---|
| Host package manager | Homebrew | Install build dependencies and FS-UAE |
| Target compiler | AmigaPorts `m68k-amigaos-gcc` | C99 compiler and AmigaOS cross-development suite |
| Target assembler | GCC integrated assembler first; VASM where useful | Compile `.S` files and optimized hand-written routines |
| Binary utilities | AmigaPorts binutils | Link, disassemble, inspect symbols, and measure binaries |
| Amiga disk tools | `amitools` | Create and inspect ADF/HDF files and Amiga Hunk binaries |
| API-level runner | `vamos` from `amitools` | Fast tests of CLI and AmigaOS API code without custom-chip emulation |
| Full emulator | FS-UAE | A1200, AGA, Kickstart, disk, audio, and integration testing |
| Toolchain host compiler | Homebrew GCC 12 | Build GCC and GDB reliably on Apple Silicon/macOS 14.1 |
| Project host compiler | Apple Clang | Native unit tests, sanitizers, and portable subsystem development |
| Build driver | GNU Make | Common entry point for host, target, packaging, and emulator tasks |
| System API reference | Amiga Developer CD 2.1 documentation | Libraries, devices, resources, Autodocs, IFF, and hardware reference material |

The maintained AmigaPorts toolchain builds GCC, binutils, AmigaOS support tools, VASM/VLink, NDK files, and several C runtimes. Its build documentation explicitly supports native Apple Silicon macOS hosts.

References:

- [AmigaPorts m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc)
- [amitools](https://github.com/cnvogelg/amitools)
- [FS-UAE documentation](https://fs-uae.net/docs/)
- [FS-UAE Homebrew formula](https://formulae.brew.sh/formula/fs-uae.html)
- [Amiga Developer CD 2.1 documentation](http://amigadev.elowar.com/read/ADCD_2.1/)

## 3. Current Host Inventory

The following components were detected on the development machine on 2026-09-02 before bootstrap:

- Apple Silicon (`arm64`);
- macOS 14.1;
- Xcode Command Line Tools and a macOS SDK;
- Homebrew under `/opt/homebrew`;
- GNU Make 4.4.1;
- Homebrew GCC 16;
- Apple Clang;
- CMake and Ninja;
- GMP, MPFR, MPC, gettext, texinfo, autoconf, wget, and XZ;
- GNU sed;
- Python 3.14 and `pipx`.

The following required or useful components were initially absent:

- Homebrew Bash;
- Automake;
- current Bison and Flex;
- GNU Coreutils, tar, and grep;
- FS-UAE;
- `amitools` and `vamos`;
- `lhasa`, which is useful for Amiga `.lha` archives.

Neither `m68k-amigaos-gcc` nor `vasmm68k_mot` was present in `PATH`.

Host bootstrap was completed on 2026-09-02. The versioned `Brewfile` is satisfied, and the following components are now installed and validated:

- Homebrew Bash 5.3;
- Automake;
- Bison 3.8 and Flex 2.6;
- GNU Coreutils, tar, grep, sed, and Make;
- FS-UAE 3.2.35;
- Lhasa;
- Homebrew GCC 12.5.0, used specifically as the native compiler for the AmigaPorts build;
- the remaining compiler-build libraries listed in the `Brewfile`.

The full AmigaPorts build was completed and smoke-tested on 2026-09-02. The installed suite includes:

- `m68k-amigaos-gcc` 16.2.0b;
- Binutils 2.46 development snapshot;
- GDB 18 development snapshot;
- VASM 2.0b;
- NDK 3.2 headers and generated interfaces;
- Newlib, libnix, Clib2, libpthread, libdebug, libstdc++, and `libnix4.library`.

The isolated Python tooling was installed and smoke-tested on 2026-09-02:

- `amitools` 0.8.1;
- `machine68k` 0.3.0;
- Python 3.13.15 inside the `pipx` environment;
- working `xdftool`, `xdfscan`, `hunktool`, `romtool`, `rdbtool`, and `vamos` commands.

The stock PAL A1200 FS-UAE profile has also been generated and launched with the local Kickstart 3.0/39.106 ROM. A local Workbench 3.0 HDF was validated with `xdfscan` and launched read-only as a development system. A valid stock Kickstart 3.1/40.068 ROM and Workbench 3.1 system are not currently available, so the 3.1 integration profiles remain pending.

## 4. Installation Layout

The compiler source and installed compiler should remain outside the project repository:

```text
$HOME/dev/toolchains/m68k-amigaos-gcc/   Toolchain source and build tree
$HOME/.local/m68k-amigaos/               Installed cross-toolchain
$HOME/.local/bin/                         pipx commands such as xdftool
$HOME/Documents/Amiga/amiga-roms/         Existing local Kickstart ROM collection
$HOME/Documents/FS-UAE/Hard Drives/       Locally supplied AmigaOS installations
```

The project repository should contain only scripts, configuration templates, version manifests, and generated-file rules. It must not contain Kickstart ROMs, Workbench files, private filesystem images, or absolute-path FS-UAE configurations.

All versioned paths MUST use `$HOME`, project-relative locations, or documented configuration variables. Developer usernames and machine-specific home-directory paths MUST remain confined to ignored local configuration and generated build files.

Using a user-writable compiler prefix avoids `sudo`, prevents accidental changes to system directories, and makes it possible to archive the finished toolchain.

## 5. Phase 1 — Install Host Dependencies

### 5.1 Homebrew packages

The complete installation command should be:

```sh
brew install \
  autoconf \
  automake \
  bash \
  bison \
  coreutils \
  flex \
  gcc@12 \
  gettext \
  gmp \
  gnu-sed \
  gnu-tar \
  grep \
  lhasa \
  libmpc \
  make \
  mpfr \
  texinfo \
  wget \
  xz \
  fs-uae
```

Homebrew safely leaves already-installed formulae in place. A future project `Brewfile` should describe this set so that a second Mac can reproduce it with `brew bundle`.

### 5.2 Build-shell environment

Apple ships BSD or older versions of several build utilities. The AmigaPorts build should use the Homebrew versions explicitly without immediately modifying the user's global shell profile:

```sh
MIGA80_BREW_PREFIX="$(brew --prefix)"

export PATH="$(brew --prefix bison)/bin:$(brew --prefix flex)/bin:$PATH"
export PATH="$(brew --prefix gcc@12)/bin:$PATH"

for pkg in coreutils gnu-sed gnu-tar grep make; do
  export PATH="$(brew --prefix "$pkg")/libexec/gnubin:$PATH"
done
```

These settings now live in `scripts/build-toolchain.sh` and `scripts/check-toolchain.sh`. Neither script modifies the user's global shell configuration.

### 5.3 Host dependency checks

Before building the compiler, a check script should verify at least:

```text
brew
gcc-12 / g++-12
gmake
bash
bison
flex
gsed
autoconf
automake
wget
git
python3
pipx
fs-uae
```

The script must print the resolved path and version of each program. This catches the common case where an old Apple-provided command appears before its Homebrew equivalent.

## 6. Phase 2 — Build the Amiga Cross-Toolchain

### 6.1 Automated build

The versioned build script performs the fetch, selects the required Homebrew tools, checks free disk space, builds NDK interfaces serially, and installs the complete suite:

```sh
./scripts/check-toolchain.sh
./scripts/build-toolchain.sh
```

Its defaults are:

```text
source: $HOME/dev/toolchains/m68k-amigaos-gcc
prefix: $HOME/.local/m68k-amigaos
NDK:    3.2
target: all
host:   gcc-12 / g++-12
```

These can be overridden with the `MIGA80_TOOLCHAIN_SOURCE`, `MIGA80_TOOLCHAIN_PREFIX`, `MIGA80_TOOLCHAIN_NDK`, `MIGA80_TOOLCHAIN_TARGET`, and `MIGA80_TOOLCHAIN_JOBS` environment variables. Set `MIGA80_TOOLCHAIN_UPDATE=1` only when intentionally advancing the fetched upstream revisions.

The source fetch performed by the script is equivalent to:

```sh
git clone https://github.com/AmigaPorts/m68k-amigaos-gcc \
  "$HOME/dev/toolchains/m68k-amigaos-gcc"

cd "$HOME/dev/toolchains/m68k-amigaos-gcc"
gmake update
```

`gmake update` fetches several upstream components. It is suitable for the initial exploratory installation but does not by itself produce a permanently reproducible build; the resulting revisions must be recorded later.

### 6.2 Manual equivalent

The initial build should include the complete suite rather than the minimum compiler target:

```sh
CC=gcc-12 \
CXX=g++-12 \
SHELL="$(brew --prefix bash)/bin/bash" \
gmake all \
  GDB_CC=gcc-12 \
  GDB_CXX=g++-12 \
  NDK=3.2 \
  PREFIX="$HOME/.local/m68k-amigaos" \
  -j"$(sysctl -n hw.logicalcpu)"
```

Reasons for this selection:

- the target is classic AmigaOS 3.0/3.1 rather than AmigaOS 4;
- NDK 3.2 provides the classic APIs needed for development, subject to the compatibility and licensing checks below;
- a full build provides binutils, VASM/VLink, Hunk support, and alternative C runtimes needed during Phase 0 evaluation;
- using Homebrew GCC as the host compiler also permits building components such as GDB that may not build correctly with Apple Clang.

No separate `make install` step is required by this toolchain: the build installs directly into `PREFIX`.

The versioned script is preferred over the abbreviated manual command. In particular, it avoids three macOS-specific failure modes found during the validated build:

- current Apple Clang and newer Homebrew GCC releases can fail while linking the large native GDB executable on macOS 14.1;
- assigning `CC=gcc-12` on the GNU Make command line leaks into runtime sub-makes and prevents libnix from selecting `m68k-amigaos-gcc`;
- a host environment value such as `DEBUG=release` is interpreted by the historical Clib2 Makefile as literal compiler input.

### 6.3 Add the installed tools to the development environment

```sh
export PATH="$HOME/.local/m68k-amigaos/bin:$PATH"
```

This should initially be performed by a project environment script. It can be added permanently to the user's shell configuration after the installation is proven stable.

### 6.4 Initial compiler validation

The compiler phase passes when all of the following succeed:

```sh
m68k-amigaos-gcc --version
m68k-amigaos-ld --version
m68k-amigaos-objdump --version
m68k-amigaos-nm --version
m68k-amigaos-size --version
vasmm68k_mot -version
```

A minimal C99 program must then be compiled with a 68020 baseline, inspected as an Amiga Hunk executable, and disassembled.

The repository provides this validation as:

```sh
./scripts/test-toolchain.sh
```

The validated result selects the `libm020` soft-float multilib, links calls to `dos.library`, and produces an AmigaOS `loadseg()`-compatible Hunk executable. It also assembles a 68020 source with VASM and verifies its Hunk object format. The current unstripped C smoke executable contains 3,524 bytes of text, 16 bytes of data, and 32 bytes of BSS.

Initial flags to evaluate are:

```text
-std=c99
-m68020
-msoft-float
-Wall
-Wextra
-Werror
-fno-common
-ffunction-sections
-fdata-sections
```

`-msoft-float` is a safeguard against generating FPU instructions for the stock A1200. MAGI-80 should avoid floating-point code in target builds altogether. Section garbage collection and the final link flags must be tested against the selected Hunk linker before becoming mandatory.

Debug builds should initially use `-O1 -g`. Release builds should compare `-Os` with `-O2`; hot paths such as C2P conversion should be selected using measurements rather than a single project-wide optimization assumption.

## 7. Phase 3 — System API and C Runtime Policy

### 7.1 General policy

MAGI-80 is allowed and encouraged to use any appropriate documented system API provided by the target machine. Calls such as `malloc()`, `free()`, `fopen()`, and `fread()` are examples of this policy, not a boundary around it.

The default engineering decision is to reuse proven AmigaOS facilities whenever they satisfy the product's compatibility, memory, performance, and runtime-mode constraints. MAGI-80 should not reimplement a general-purpose operating-system service merely to appear more like a standalone kernel.

Candidate facilities include, but are not limited to:

- Exec memory allocation, lists, tasks, signals, messages, ports, semaphores, interrupts, and cache-control functions;
- DOS file handles, locks, directories, volume enumeration, notifications, processes, and filesystem handlers;
- Intuition, GadTools, graphics, layers, and related display services while MAGI-80 is in hosted editing mode;
- timer, input, keyboard, gameport, audio, console, clipboard, and trackdisk devices where they provide the required semantics;
- system resources such as CIA and misc resources when lower-level ownership is necessary and can be acquired safely;
- datatypes, locale, IFF, and other installed libraries when their availability and footprint match the minimum target;
- C runtime wrappers where they are smaller or clearer than calling the corresponding AmigaOS interface directly.

The [Amiga Developer CD 2.1 documentation](http://amigadev.elowar.com/read/ADCD_2.1/) is the primary online programming reference for this work. It includes the ROM Kernel Reference Manuals, Devices manual, Hardware Reference Manual, Includes and Autodocs, IFF material, and Amiga Mail technical articles.

The documentation collection also contains material for releases newer than the minimum MAGI-80 target. Every selected function must therefore be checked against the exact library or device version present on Kickstart/Workbench 3.0 and 3.1. Compilation against an NDK header is not proof that the function exists on both target systems.

### 7.2 API-use constraints

System API reuse remains subject to the following rules:

- use public, documented interfaces rather than private structures or undocumented ROM entry points;
- open each library or device with a minimum version consistent with the exact functions used, and handle failure cleanly;
- preserve the stock A1200 path: no API may silently introduce a Fast RAM, FPU, accelerator, or later-OS dependency;
- use AmigaDOS filesystem APIs for mounted OFS and FFS volumes rather than parsing their raw structures in MAGI-80;
- allocate DMA-visible graphics and audio data in Chip RAM with the appropriate Exec flags;
- keep blocking, allocating, filesystem, and other scheduler-dependent calls out of exclusive runtime sections where AmigaOS scheduling is forbidden;
- acquire and release devices, resources, interrupts, display ownership, and audio channels symmetrically;
- prefer the hosted OS service for editing and project management, but permit measured direct-hardware code for the game runtime, C2P, Copper, blitter, input, and Paula paths;
- verify restoration and error paths under both supported OS versions.

This is the practical meaning of MAGI-80 “replacing AmigaOS” for the user without pointlessly replacing the mature services already present in the machine.

### 7.3 Selected C runtime and initial ABI lock

The cross-toolchain offers multiple C runtime choices, including `libnix`, `clib2`, and `newlib`. The selected runtime is only one layer of the system API policy: code may freely mix suitable C library functions with direct AmigaOS library and device calls.

The initial Phase 0 runtime is **libnix**, selected with `-mcrt=nix20`. AmigaPorts requires a runtime selector to occur exactly once and at the end of the compiler link command. This selection targets the Kickstart 2+ libnix startup and does not add a shared runtime dependency to the target system.

The comparison program exercises:

- process startup and clean exit;
- `malloc()`, `realloc()`, and `free()`;
- `calloc()` and the observed `malloc(0)` behavior;
- file creation, reading, writing, seeking, renaming, and deletion;
- AmigaDOS error propagation;
- command-line arguments and current-directory behavior;
- representative direct calls to Exec and DOS;
- allocation of ordinary memory versus DMA-visible Chip RAM;
- API-level execution through `vamos`;
- execution on the stock A1200 FS-UAE Kickstart 3.0 profile;
- executable and static-data size.

The comparison performed on 2026-09-02 produced the following minimal-bootstrap measurements:

| Runtime | Selector | Hunk bytes | Stripped bytes | Text | Data | BSS | `vamos` |
|---|---|---:|---:|---:|---:|---:|---|
| newlib | default | 5,176 | 3,624 | 3,240 | 16 | 32 | pass |
| **libnix** | `-mcrt=nix20` | **3,772** | **2,244** | **1,756** | **32** | **40** | pass |
| clib2 | `-mcrt=clib2` | 20,108 | 15,856 | 13,252 | 108 | 1,024 | pass |

The allocation and filesystem program produced:

| Runtime | Required API | Hunk bytes | Text | BSS | `vamos` | FS-UAE KS3.0 |
|---|---|---:|---:|---:|---|---|
| newlib compatibility subset | fail | 79,888 | 68,580 | 780 | subset only | fail |
| **libnix** | **pass** | **18,576** | **13,876** | **120** | **pass** | **pass** |
| clib2 | pass | 31,884 | 24,264 | 1,024 | pass | pass |

Newlib is not eligible for the initial runtime lock because the installed port has three concrete problems in this matrix:

- ISO C `rename()` fails to link because its fallback references an undefined `_link` stub;
- opening a missing file does not propagate a non-zero `errno` in the tested configuration;
- command-line parsing has to be forced into the link, after which formatted output pulls in `mathieeedoubbas.library` and the minimal stock boot fixture stops because that library is unavailable, despite the soft-float target.

Clib2 remains a valid fallback: it passed the required behaviors, but its minimal executable was 16,336 bytes larger than libnix and its functional executable was 13,308 bytes larger. Libnix therefore supplies the best validated combination of required semantics and floppy/Chip-RAM economy.

The initial system build ABI is locked as one compatibility unit:

- GCC and component revisions recorded in `toolchain/versions.lock`;
- C99 source mode;
- AmigaOS Hunk executable format;
- 68020 CPU baseline;
- software floating point, with floating-point use excluded from MAGI-80 code;
- libnix Kickstart 2+ startup and runtime via `-mcrt=nix20`;
- default non-base-relative data model and startup stack policy;
- no ixemul, shared libc, FPU, Fast RAM, or later-OS dependency;
- direct documented AmigaOS calls permitted alongside the C runtime.

For each runtime, the comparison retains:

- the unstripped executable;
- the stripped executable;
- a link map;
- `nm` and `size` output;
- `objdump` disassembly;
- exact `vamos` and FS-UAE observations.

Run `gmake runtime-compare` to regenerate `build/runtime-comparison/summary.md` and its detailed maps, disassembly, symbols, sizes, and logs. Kickstart 3.1, stack high-water, process memory deltas, later direct APIs, and a real stock A1200 remain compatibility gates. A failure at one of those gates requires an explicit decision-log revision rather than a silent runtime change.

## 8. Phase 4 — Install Amiga Disk and Hunk Tools

`amitools` supplies the required host-side utilities:

- `xdftool` creates and modifies OFS/FFS ADF and HDF images;
- `xdfscan` checks disk images and directory trees;
- `hunktool` inspects Amiga Hunk executables and objects;
- `vamos` runs many CLI AmigaOS programs through an API-level emulator;
- `rdbtool` handles hard-disk RDB structures when needed later.

The host default is Python 3.14, but the native `machine68k` dependency is deliberately isolated under Homebrew Python 3.13. Install that interpreter once:

```sh
brew install python@3.13
```

Then create the isolated environment:

```sh
pipx install \
  --python "$(brew --prefix python@3.13)/bin/python3.13" \
  "amitools[vamos]"
```

This combination built a native arm64 wheel successfully and avoids making the development environment depend on unverified Python 3.14 extension compatibility. The installed package versions can be inspected with:

```sh
pipx runpip amitools show amitools machine68k
```

Validation commands:

```sh
xdftool --help
xdfscan --help
hunktool --help
vamos --help
./scripts/test-amiga-tools.sh
```

The automated test compiles the C99 smoke program, validates its loadseg structure with `hunktool`, executes it under `vamos -C 20`, creates 880 KiB OFS and FFS images, writes a file to each, and validates both filesystems with `xdfscan`.

`vamos -C 20` should be used for fast tests of parsers, file operations, compiler stages, and other CLI-compatible code. It cannot validate AGA registers, Copper lists, blitter behavior, Paula audio, CIA timing, display DMA contention, or exclusive AmigaOS takeover.

## 9. Phase 5 — Configure FS-UAE

### 9.1 Required local Amiga files

FS-UAE requires legally obtained Kickstart ROM images. A local ROM collection already exists at:

```text
$HOME/Documents/Amiga/amiga-roms
```

MAGI-80 also needs legally obtained Workbench/AmigaOS files for its hosted development and boot workflows.

The resulting local layout is:

```text
$HOME/Documents/Amiga/amiga-roms/
$HOME/Documents/Amiga/Workbench V3.0 (1992)(Commodore).hdf
$HOME/Documents/FS-UAE/Hard Drives/Workbench-3.0/
$HOME/Documents/FS-UAE/Hard Drives/Workbench-3.1/
```

The standalone Workbench 3.0 HDF is the current convenience system. It has a valid OFS structure and a Workbench 3.0 startup sequence, but it is an old customized image with little free space, not the future clean reference installation. The two directories under `FS-UAE/Hard Drives` are the recommended eventual locations for clean 3.0 and 3.1 systems.

These files must never be added to the repository or copied into generated public artifacts without confirmed redistribution rights.

The repository provides an ignored local configuration file containing the exact selected ROM paths, without assuming filenames inside the collection. Start from the versioned example:

```sh
cp config/fs-uae/local.env.example config/fs-uae/local.env
```

Then set either or both of the following path/checksum pairs:

```sh
MAGI80_KICKSTART_30="/absolute/path/to/a1200-kickstart-39.106.rom"
MAGI80_KICKSTART_30_SHA256="optional-local-sha256"
MAGI80_KICKSTART_31="/absolute/path/to/a1200-kickstart-40.068.rom"
MAGI80_KICKSTART_31_SHA256="optional-local-sha256"
```

The generator requires at least one ROM, verifies its size, optional SHA-256 digest, internal ROM checksum, and exact revision with `romtool`, then resolves it into concrete FS-UAE files under the ignored `build/fs-uae/` directory. ROM files do not need to be copied into FS-UAE's default Kickstarts directory.

The current local collection contains a valid Kickstart 3.0/39.106 image. Its file labelled 3.1/40.069 is deliberately not selected: FS-UAE reports it as an unknown ROM, and its fingerprint corresponds to the modified Harry Sintonen version-number image rather than the stock Commodore A1200 3.1/40.068 ROM. The project must obtain a legally licensed, unmodified 40.068 image before certifying the 3.1 profile. See the [Kickstart 3.1 version notes](https://www.lemonamiga.com/page/kickstart-rom-3-1) and the [matching TOSEC record](https://data.spludlow.co.uk/tosec/tosec/Commodore%20Amiga%20-%20Firmware/Kickstart%20v3.1%20r40.068%20%281993-12%29%28Commodore%29%28A1200%29%5Bf%20Harry%20Sintonen%5D%5Bh%20version%20number%5D).

### 9.2 Certified emulator model

The base configuration must use `A1200`, not `A1200/020`:

```ini
[fs-uae]
amiga_model = A1200
chip_memory = 2048
fast_memory = 0
ntsc_mode = 0
kickstart_file = @KICKSTART_ROM@
```

In FS-UAE, `A1200` represents the standard A1200. `A1200/020` substitutes a full 68020 and permits configurations such as Zorro III memory that are not representative of a stock 68EC020 machine.

The configuration must not enable:

- Fast RAM;
- an accelerator;
- an FPU;
- JIT execution;
- an NTSC display mode for the certified PAL configuration.

Explicit values are preferred even when they match FS-UAE defaults. This prevents a future emulator-default change from silently altering the certified machine.

### 9.3 Configuration matrix

Two ROM-only profiles provide the first configuration and Kickstart checks:

| Profile | Purpose |
|---|---|
| `a1200-pal-ks30-rom` | Validate the stock machine with Kickstart 3.0/39.106 |
| `a1200-pal-ks31-rom` | Validate the stock machine with Kickstart 3.1/40.068 |

Four media-backed configurations are maintained as templates:

| Profile | Kickstart/Workbench | Boot source | Purpose |
|---|---|---|---|
| `a1200-pal-ks30-hd` | 3.0 | Local system drive plus mounted build directory | Fast development |
| `a1200-pal-ks31-hd` | 3.1 | Local system drive plus mounted build directory | Fast compatibility testing |
| `a1200-pal-ks30-adf` | 3.0 | Generated MAGI-80 ADF | Floppy boot validation |
| `a1200-pal-ks31-adf` | 3.1 | Generated MAGI-80 ADF | Floppy boot validation |

An optional Fast RAM profile may be added later, but it must never replace the zero-Fast-RAM test profiles.

### 9.4 Fast hard-disk development loop

FS-UAE can mount a macOS directory as an Amiga drive. The development profile should mount:

1. a local licensed AmigaOS installation as the boot drive;
2. the project staging directory as a second drive.

Template example:

```ini
[fs-uae]
amiga_model = A1200
chip_memory = 2048
fast_memory = 0
ntsc_mode = 0
kickstart_file = @KICKSTART_ROM@
hard_drive_0 = @AMIGAOS_SYSTEM_DIRECTORY@
hard_drive_0_read_only = 1
hard_drive_1 = @PROJECT_BUILD_DIRECTORY@
```

The licensed system HDF is mounted read-only to keep emulator tests from altering the source image; the generated staging directory remains writable. FS-UAE documents both HDF and host-directory mounts, including the per-drive read-only option, in its [hard-drive configuration reference](https://github.com/FrodeSolheim/fs-uae/blob/main/docs/options/hard_drive_0). This loop avoids rebuilding an ADF for every edit:

```text
edit -> host tests -> cross-compile -> launch FS-UAE -> run from mounted directory
```

### 9.5 Floppy validation loop

The distribution profile should use the exact generated image:

```ini
[fs-uae]
amiga_model = A1200
chip_memory = 2048
fast_memory = 0
ntsc_mode = 0
kickstart_file = @KICKSTART_ROM@
floppy_drive_0 = @MAGI80_ADF@
```

The ADF loop is:

```text
cross-compile -> stage files -> create ADF -> verify ADF -> boot ADF in FS-UAE
```

Both OFS and FFS candidate images should be generated during Phase 0. The selected release format must be based on measured payload capacity, boot behavior, memory use, compatibility, and disk-write requirements.

### 9.6 Generate, check, and run the profiles

Generate every profile whose local dependencies are available:

```sh
./scripts/configure-fs-uae.sh
./scripts/test-fs-uae-config.sh
```

The generator removes its six known generated files before rebuilding them, preventing stale profiles after a local ROM or media path is removed. It skips an unavailable Kickstart branch, Workbench system, or ADF with an explicit status rather than substituting another machine configuration.

Launch the currently validated ROM-only profile with:

```sh
./scripts/run-fs-uae.sh a1200-pal-ks30-rom
```

The 2026-09-02 ROM launch was confirmed in the FS-UAE log as an A1200 in PAL mode at 50 Hz, with a 68020-class CPU, no FPU, no JIT, 2 MiB Chip RAM, no Fast RAM, and recognized Kickstart 3.0/39.106. The HD launch additionally confirmed a read-only `DH0:` hardfile and writable `DH1:MAGI80` host-directory mount, followed by the Workbench interlaced PAL display mode.

The full-system smoke harness then booted a generated OFS test ADF, ran the staged `MAGI80:magi80` Hunk executable, redirected its AmigaDOS output to `MAGI80:fs-uae-smoke.out`, and compared that host-visible file with a golden result. This validates the complete cross-compile-to-FS-UAE loop without modifying the Workbench HDF or adding proprietary data to the test disk.

The hosted AGA display and reference chunky-to-planar path are now validated. Exclusive takeover, Paula output, optimized-converter timing, and real-hardware behavior remain unvalidated. The current automated launch environment emitted OpenAL source errors; audio must therefore remain a separate pending smoke test.

## 10. Phase 6 — Project Build Integration

The compiler phase has added the following versioned files:

```text
Brewfile

toolchain/
  versions.lock

scripts/
  check-toolchain.sh
  build-toolchain.sh
  test-amiga-tools.sh
  test-toolchain.sh

tests/smoke/
  assembler/
    minimal.s
  c99-runtime/
    main.c
```

The FS-UAE phase adds the following versioned files:

```text
scripts/
  configure-fs-uae.sh
  run-fs-uae.sh
  test-fs-uae-config.sh

config/fs-uae/
  local.env.example
  a1200-pal-ks30-rom.fs-uae.in
  a1200-pal-ks31-rom.fs-uae.in
  a1200-pal-ks30-hd.fs-uae.in
  a1200-pal-ks31-hd.fs-uae.in
  a1200-pal-ks30-adf.fs-uae.in
  a1200-pal-ks31-adf.fs-uae.in
```

The first application-build slice adds:

```text
Makefile

src/
  graphics/
    c2p_reference.c
    c2p_reference_layouts.c
    c2p_reference.h
  main.c

scripts/
  compare-c-runtimes.sh
  test-fs-uae-runtime.sh
  validate-c2p-benchmark-report.sh

tests/smoke/
  aga-screen/
    main.c
    expected.txt
  c-runtime-matrix/
    main.c
  hosted-bootstrap/
    expected.txt
  fs-uae/
    Startup-Sequence

tests/host/
  c2p-reference/
    main.c
    expected.txt

tests/benchmark/
  c2p-layouts/
    main.c
```

The remaining proposed integration files and directories are:

```text
toolchain/
  README.md

scripts/
  bootstrap-macos.sh
  make-adf.sh

tests/smoke/
  amiga-libraries/
  paula-audio/

build/
  amiga/
  staging/
  disks/
  reports/
```

`build/` and local generated FS-UAE configurations should be ignored by Git.

The top-level GNU Make interface now exposes:

| Target | Result |
|---|---|
| `gmake amiga` | Build `build/amiga/magi80`, an AmigaOS Hunk executable |
| `gmake stage` | Copy it into the host directory mounted as `MAGI80:` |
| `gmake inspect` | Generate size, symbol, disassembly, and link-map reports |
| `gmake vamos-test` | Execute it with `vamos -C 20` and compare its output |
| `gmake run` | Stage it and launch the Workbench 3.0 HD profile interactively |
| `gmake fs-uae-smoke` | Build, stage, boot the local harness, execute from `MAGI80:`, and check the result |
| `gmake c2p-test` | Compile the portable C99 converter natively and compare its output with byte-exact golden vectors |
| `gmake c2p4-test` | Validate packed4/byte4 scalar, pair-LUT, and table-free mask32 contracts against canonical output |
| `gmake graphics-reference-test` | Compile the portable three-layer compositor natively and verify canonical PLANAR/PIXEL/OBJECTS golden output |
| `gmake aga-reference-test` | Decode eight dual-playfield bitplanes back to canonical pixels and verify the complete compositor → C2P → decoder differential |
| `gmake graphics-report-test` | Validate positive and negative fixtures for the common Phase 0 graphics benchmark report schema |
| `gmake aga-screen-smoke` | Convert a chunky framebuffer into a hosted PAL 256 × 256 AGA dual-playfield screen, validate it, close it, and repeat under FS-UAE |
| `gmake c2p-benchmark` | Build the 68020 reference layout benchmark without launching an emulator |
| `gmake c2p-benchmark-fs-uae` | Measure all four reference layouts with E-Clock timing and display DMA active, then validate and retain the report |
| `gmake c2p4-benchmark` | Build the single-layer four-plane C99 pair-LUT, 68020 pair-LUT/mask32, and staged blitter-publication benchmark without launching an emulator |
| `gmake c2p4-benchmark-fs-uae` | Run all 24 hosted C2P4 cases with active display DMA, require exact canonical output, account for minimum traffic, and validate the common graphics report |
| `gmake runtime-compare` | Build, inspect, and execute the newlib, libnix, and clib2 runtime matrix through `vamos` and FS-UAE |
| `gmake check` | Run the native C2P, compositor, inverse AGA decoder, and graphics-report tests, inspection, `vamos`, and the complete FS-UAE integration smoke tests |
| `gmake clean` | Remove only generated application, report, harness, and staging outputs |

Future targets will add `check-tools`, further native host tests, the release ADF, and packaging once those layers exist.

The current bootstrap is compiled as C99 with `-m68020 -msoft-float -Os`, warnings as errors, a link map, and the final runtime selector `-mcrt=nix20`. It is a 3,772-byte file with 1,756 bytes of text, 32 bytes of data, and 40 bytes of BSS. It calls `dos.library` directly for output and introduces no floating-point requirement.

The build must fail clearly when a proprietary local input is absent. It must never silently download, copy, or package a Kickstart ROM or Workbench component.

## 11. Testing and Debugging Strategy

### 11.1 Native host tests

The C99 reference C2P converters now compile natively with Apple Clang and pass explicit byte-level vectors for bit ordering, playfield assignment, source-layout equivalence, strides, padding preservation, full-frame output, and invalid arguments. The current C2P4 suite additionally covers the three architecture viewport profiles, single-layer packed4 and byte4 sources, scalar/pair-LUT/mask32 paths, the mask32 multiple-of-32 contract, and exact compositor → C2P4 → AGA decoder output. The target-compiled benchmarks exercise both real assembly cores and display planes under FS-UAE. See [Reference Chunky-to-Planar Converter](./c2p-reference.md) for the historical two-layer contract and [Four-Plane C2P Reference and Benchmark](./c2p4-benchmark.md) for current work.

The following additional portable components should also compile natively with Apple Clang:

- MAGI Lua lexer, parser, and type checker;
- typed intermediate representation;
- 68020 instruction encoder and relocation logic;
- cartridge parser and serializer;
- MOD parser and reference tick engine;
- fixed-point math;
- reference renderer;
- compression and dictionary data structures.

Host debug tests should use AddressSanitizer and UndefinedBehaviorSanitizer where possible. Host execution is the fastest place for fuzzing and differential tests, but it cannot validate big-endian mistakes unless the tests use explicit byte-level golden data.

### 11.2 Cross-binary inspection

Every significant target build should retain:

- a linker map;
- sorted symbols by size;
- section and total sizes;
- disassembly of generated code;
- an unstripped executable for diagnosis;
- a stripped executable for the floppy budget.

This makes unexpected runtime pulls, floating-point helpers, large `stdio` dependencies, or accidental data growth visible early.

### 11.3 API-level execution

`vamos` is appropriate for frequent tests of target-compiled code that uses supported Exec and DOS calls. It provides a useful layer between native host tests and full machine emulation.

Hardware-facing code must be excluded or replaced behind narrow platform interfaces in these tests.

### 11.4 Full-system emulation

FS-UAE is required for:

- AmigaOS process startup and shutdown;
- Kickstart 3.0/3.1 compatibility;
- Chip RAM allocation and fragmentation scenarios;
- AGA display and Copper setup;
- input devices;
- Paula MOD playback;
- hard-disk and floppy workflows;
- hosted-to-exclusive-mode transitions;
- repeated restoration of AmigaOS state.

The first hardware-facing regression is now `gmake aga-screen-smoke`. It validates the AGA-capable PAL dual-playfield display database entry, an exact 256 × 256 × 8 Intuition screen, eight displayable Chip-RAM planes, PF1/PF2 palette bases 0/16, conversion of a 65,536-byte two-layer chunky buffer into those planes, representative pixel readback, and a stable repeated close/reopen/close cycle. It remains an OS-managed hosted test and does not validate exclusive takeover or performance.

`gmake graphics-reference-test` is the host-side logical oracle for the new three-layer direction. It composes a native-planar base, a positioned transparent pixel viewport, and priority-ordered virtual objects into canonical palette identities. Hardware-sprite and planar-fallback hints must produce identical pixels. This test validates semantics only and performs no AGA emulation. See [Three-Layer Graphics Reference Compositor](./graphics-reference-compositor.md).

`gmake aga-reference-test` closes the playfield side of the differential loop. It reconstructs canonical palette identities from BPL1–BPL8 and requires a deterministic full frame to survive compositor → C2P → decoder unchanged. It does not yet reconstruct hardware-sprite DMA output. See [AGA Dual-Playfield Reference Decoder](./aga-reference-decoder.md).

`gmake graphics-report-test` validates the common machine-readable contract that all new four-plane, planar, scrolling, object, and combined-scene benchmarks must emit. It requires explicit DMA/timing authority, phase and total timings, memory/workload counts, and equal oracle/candidate checksums. See [Graphics Benchmark Report Format](./graphics-benchmark-report-format.md).

`gmake c2p-benchmark-fs-uae` adds a machine-readable E-Clock protocol for four source layouts. It forces source storage into Chip RAM, writes the active screen planes, and verifies identical checksums. Its scalar reference results validate the harness only; FS-UAE timing is not release evidence and cannot select the runtime layout. See [Chunky Layout and C2P Benchmark](./c2p-layout-benchmark.md).

`gmake c2p4-benchmark-fs-uae` runs the architecture-aligned 24-case matrix: three viewport sizes, packed4 and byte4, C99 pair-LUT, real 68020 pair-LUT and table-free mask32 assembly, and a staged Chip-RAM-to-PF1 `BltBitMap()` publication control. Every case reconstructs the complete canonical image, reports a conservative traffic lower bound and `timing_scope=hosted_cooperative`, and validates the common graphics schema. The staged copy is not a hybrid converter, direct visible-screen writes are not safe publication, and emulator E-Clock values remain protocol-only. `WaitTOF()` is outside each measured bracket but still makes this cooperative harness unsuitable for fine timing. See [Four-Plane C2P Reference and Benchmark](./c2p4-benchmark.md).

### 11.5 Real hardware

FS-UAE is not the release authority for:

- active-display Chip RAM contention;
- exact Copper/blitter timing;
- CIA behavior;
- Paula timing and audible output;
- physical floppy reliability;
- cache synchronization of generated 68020 code;
- restoration after long run/stop stress sessions.

Real A1200 validation remains required. Transfer can initially use a Gotek, PCMCIA/CompactFlash, network transfer, or a flux-capable floppy interface. A normal USB PC floppy drive cannot generally write Amiga DD media in the required raw format.

The hardware performance harness must be a separate bounded exclusive-runtime executable. It prepares files and allocations under AmigaOS, saves and acquires the required resources, freezes scheduling, owns a minimal VBlank/raster interrupt path, batches iterations without per-sample OS waits, and restores state on every recoverable exit. Stack canaries and durable phase/progress markers are required because an automation timeout alone cannot distinguish a long run from a deadlock, crash, or teardown fault. Hosted tests remain the frequent correctness and restoration gate.

## 12. Reproducibility Plan

The first successful installation is recorded in `toolchain/versions.lock`. It currently captures:

- macOS and Xcode Command Line Tools versions;
- Homebrew prefix and formula versions;
- AmigaPorts top-level commit;
- commits of every fetched compiler, binutils, library, and support component;
- compiler target and configured prefix;
- the NDK selection and source archive checksum;
- FS-UAE version;
- the Python, `amitools`, and `machine68k` versions;
- the smoke-test architecture, floating-point, format, and size results.

The manifest now records the selected C runtime, selector, target ABI, runtime-matrix outcomes, and current bootstrap sizes. It must still be extended after the remaining phases with checksums of the packaged cross-toolchain and generated release files.

The installed `$HOME/.local/m68k-amigaos` tree should be packaged after validation. Keeping this private archive plus its checksum is more reliable than assuming that a future `make update` will reproduce the same upstream component revisions.

Kickstart and Workbench files should be identified in the local configuration by checksum, but the files and their checksums should not be required in public CI unless licensing permits it.

## 13. Licensing Gates

Before publishing a bootable ADF or distributing the toolchain, the project must resolve:

1. whether the selected NDK material can be redistributed as part of a binary toolchain archive;
2. whether any VASM, VLink, or VBCC distribution restriction affects the proposed toolchain package;
3. which AmigaDOS boot files, if any, are required on the MAGI-80 floppy;
4. whether those boot files may be redistributed;
5. whether the public release must instead provide an installer that copies licensed files from the user's Workbench media;
6. whether a compatible redistributable replacement can legally and technically satisfy the boot requirements.

Compiler output and the MAGI-80 program are separate from permission to redistribute compiler components or AmigaOS system files. No public ADF should be produced until this gate is closed.

## 14. Main Setup Risks

| Risk | Consequence | Mitigation |
|---|---|---|
| Homebrew GNU tools are not first in `PATH` | Toolchain build fails in confusing ways | Use a checked project environment script and print resolved tool paths |
| Latest AmigaPorts branches change | Previously working builds become irreproducible | Record all commits and archive a validated prefix |
| Python 3.14 is too new for `machine68k` | `vamos` cannot be installed | Keep the validated Python 3.13 `pipx` environment |
| A comfortable emulator profile hides stock-machine failures | MAGI-80 works only with Fast RAM or faster CPU settings | Certify only explicit A1200, 2 MiB Chip, zero Fast RAM profiles |
| Selected libc pulls large dependencies | Executable exceeds the floppy or memory budget | Compare runtimes immediately and inspect every link map |
| NDK headers allow calls newer than OS 3.0/3.1 | Runtime failure on a stock system | Audit library versions and test every smoke program on both OS versions |
| Emulator success is treated as hardware proof | Timing and cache bugs escape | Keep real-A1200 gates for graphics, DMA, audio, takeover, and generated code |
| Bootable ADF includes proprietary system files | Release cannot be distributed | Keep licensed inputs local and design an installer or legal alternative |

## 15. Execution Order and Current State

The installation should be performed in small, independently verifiable steps:

1. [x] Install the missing Homebrew packages and FS-UAE.
2. [x] Add a non-mutating `check-toolchain.sh` and verify paths and versions.
3. [x] Build the complete AmigaPorts suite into the user-local prefix.
4. [x] Compile and inspect a minimal 68020 soft-float C99 executable.
5. [x] Install `amitools`; validate `xdftool`, `hunktool`, and `vamos`.
6. [ ] Configure legal Kickstart assets outside the repository: 3.0/39.106 is complete; stock 3.1/40.068 is pending.
7. [x] Add, generate, inspect, and launch the stock-A1200 Kickstart 3.0 ROM-only profile.
8. [x] Add the Kickstart 3.1 ROM-only template and all four HD/ADF templates.
9. [ ] Prepare clean reference Workbench systems: the existing 3.0 HDF/profile works read-only; 3.1 is pending.
10. [x] Complete the mounted-directory loop by staging and running a target executable from `MAGI80:`.
11. [x] Compare C runtimes and freeze the initial target ABI on libnix/`-mcrt=nix20`; Kickstart 3.1 and real-hardware revalidation remain gates.
12. [x] Generate and verify OFS and FFS test ADFs.
13. [ ] Generate a MAGI-80 ADF profile and boot it under both target Kickstart versions.
14. [ ] Add the first AGA, input, and Paula smoke tests: the hosted AGA screen, four-layout historical C2P equivalence, and 24-case optimized C2P4 correctness/report protocol pass on FS-UAE/Kickstart 3.0; input, Paula, exclusive timing/display ownership, Kickstart 3.1, and authoritative hardware timing remain pending.
15. [ ] Package and checksum the validated compiler prefix.
16. [ ] Complete the version manifest with later-phase versions and archive checksums.
17. [ ] Re-run the smoke-test sequence on a real stock A1200.

## 16. Definition of Done

The macOS development toolchain is complete when:

- a fresh shell can activate a documented project environment;
- every required host tool is detected with the expected version;
- `m68k-amigaos-gcc` builds a C99 Hunk executable for a 68020 without FPU requirements;
- target assembly can be built through GCC and VASM;
- binary size, symbols, relocations, and disassembly can be inspected;
- the selected libc passes its file, allocation, startup, and compatibility tests;
- compatible target tests run through `vamos -C 20`;
- FS-UAE launches explicit PAL A1200 configurations with exactly 2 MiB Chip RAM and no Fast RAM;
- the executable runs from a mounted host directory under AmigaOS 3.0 and 3.1;
- reproducible OFS and FFS ADF candidates can be created and verified;
- the selected ADF boots under both target configurations;
- ROM and Workbench files remain outside Git and outside public artifacts;
- all compiler, emulator, Python-tool, NDK, runtime, and build revisions are recorded;
- the toolchain prefix can be archived and restored using a verified checksum;
- the first hardware-facing smoke test also passes on a real stock A1200.
