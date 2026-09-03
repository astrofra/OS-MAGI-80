# MAGI-80 Product Requirements, Technical Specification, and Development Roadmap

| Field | Value |
| --- | --- |
| Document status | Working specification, revision 0.1 |
| Date | 2026-09-02 |
| Target release | MAGI-80 1.0 |
| Primary hardware | Stock PAL Amiga 1200, 68EC020, AGA, 2 MiB Chip RAM |
| Implementation | C99 with narrowly scoped 68020 assembly |
| Build model | Cross-compiled on a modern host, preferably with GCC |

## 1. Purpose

This document defines MAGI-80, a small fantasy-console-style development environment for a stock Amiga 1200. It turns the machine into a focused place for creating and running games and demos, with deliberate limits inspired by PICO-8.

MAGI-80 provides:

- an integrated source-code editor;
- an integrated sprite editor;
- ProTracker module import and playback;
- a strongly typed, Lua-like game language compiled on the Amiga directly to native 68020 code;
- a 256 × 256, two-layer chunky graphics API transparently converted to AGA planar graphics;
- project and cartridge storage through AmigaDOS;
- a reversible exclusive runtime mode that freezes AmigaOS task scheduling while a game runs;
- both bootable-floppy and hard-disk launch paths.

This specification is intended to be detailed enough to guide architecture, implementation, validation, and scope decisions. Numeric budgets are initial engineering targets. Phase 0 must measure them on a real stock A1200 before they become release commitments.

## 2. Normative language

The terms **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** describe requirement priority:

- **MUST** is required for MAGI-80 1.0.
- **SHOULD** is expected unless Phase 0 evidence shows that the stock machine cannot support it reliably.
- **MAY** is optional or belongs to a later release.

## 3. Product definition

### 3.1 What MAGI-80 is

MAGI-80 is a fantasy computer and integrated development environment hosted by AmigaOS while editing, with an exclusive hardware-oriented execution mode while running a cartridge. It replaces the normal AmigaOS user experience from the user's perspective, but it does not replace Kickstart, Exec, AmigaDOS, or the installed filesystem implementation in version 1.0.

The product may be called a mini-OS because it supplies its own shell, editors, language, runtime, graphics model, and asset format. Technically, version 1.0 is a single AmigaOS executable with two operating personalities:

1. **Hosted mode:** AmigaOS continues multitasking and provides filesystems, devices, memory allocation, and system-friendly display/input services.
2. **Exclusive runtime mode:** the MAGI-80 task prevents task rescheduling, takes ownership of the display, blitter, input path, and Paula audio resources needed by the cartridge, and makes no filesystem or other blocking OS calls until it restores the hosted environment.

This interpretation resolves an otherwise irreconcilable requirement: a completely independent bare-metal OS cannot use AmigaOS libraries to read OFS and FFS volumes. A future bare-metal edition would need its own filesystem and device drivers and is outside the 1.0 scope.

MAGI-80 MUST reuse AmigaOS and the selected lightweight C runtime wherever that improves reliability without weakening the fantasy-machine contract. In hosted mode, ordinary C99 allocation and stream APIs such as `malloc()`, `free()`, `fopen()`, `fread()`, `fwrite()`, and `fclose()` are valid implementation choices when their runtime is compatible with the target systems. Direct Exec, DOS, graphics, device, and resource APIs remain appropriate where MAGI-80 needs Amiga-specific capabilities such as volume enumeration, Chip RAM, screen ownership, or error details. Reimplementing a general allocator, filesystem, device layer, or window system is not a goal.

### 3.2 PICO-8 principles adopted

MAGI-80 adopts the following general principles rather than attempting API compatibility:

- one coherent machine with fixed capabilities;
- immediate edit–compile–run–stop feedback;
- source, sprites, maps, palette, and music collected into a shareable cartridge;
- intentionally small resource limits that encourage complete projects;
- a compact built-in API rather than a general-purpose operating system API;
- deterministic behavior and visible resource meters;
- creation on the target machine, without requiring a modern host after installation;
- easy escape from a running program back to the editor.
- maximum useful leverage of AmigaOS during creation, with direct hardware access reserved for capabilities and performance that the runtime actually needs.

### 3.3 Product values

When requirements compete, decisions SHOULD favor, in order:

1. safe restoration of AmigaOS and user data;
2. correct operation on a stock 2 MiB A1200;
3. a short and predictable creation loop;
4. deterministic cartridge behavior;
5. a small disk and memory footprint;
6. peak graphics performance;
7. optional convenience features.

### 3.4 Native-hardware leverage policy

MAGI-80 SHOULD use the machine's existing strengths instead of simulating in software what the A1200 already does well. The intended split is:

- Exec, AmigaDOS, the C runtime, Intuition/graphics services, device APIs, and resource arbitration while editing;
- the 68020's 32-bit operations for generated game logic, drawing, fixed-point math, and C2P staging;
- AGA bitplane DMA, 4+4 dual playfields, palette banks, fetch modes, and the Copper for final display;
- the blitter for clears, copies, masks, or planar work only where measurement shows a gain;
- Paula DMA for the four ProTracker channels;
- CIA and standard ports for precise timing and input when safely owned;
- optional hardware sprites for the MAGI-80 pointer or diagnostic overlay, without changing cartridge sprite semantics.

The fantasy-machine API remains stable even when a backend optimization changes. A feature is not considered “hardware-accelerated” until it is faster on a real stock A1200 with active display DMA.

## 4. Goals and non-goals

### 4.1 Version 1.0 goals

- Boot or launch on a stock A1200 with no accelerator, Fast RAM, hard disk, or FPU.
- Fit a useful MAGI-80 distribution, including one example cartridge, on one standard Amiga DD floppy.
- Launch the same core program from an AmigaOS hard disk.
- Keep AmigaOS alive and schedulable throughout editing.
- Enter and leave exclusive runtime mode repeatedly without rebooting or damaging the prior display, audio, input, or filesystem state.
- Compile strongly typed MIGA Lua source directly to bounded native 68020 code on the Amiga itself.
- Render two logical 4-bit chunky layers as an AGA 4+4-bitplane dual-playfield display.
- Provide 16 opaque background colors and 15 opaque foreground colors selected from a deliberately restricted 12-bit RGB space of 4,096 colors.
- Import and play conventional four-channel, 31-sample ProTracker modules.
- Read projects and assets from mounted OFS and FFS volumes through AmigaDOS.
- Offer a usable code editor and sprite editor at the virtual resolution.
- Provide deterministic failure handling for compile errors, runtime errors, invalid files, and insufficient resources.

### 4.2 Explicit non-goals for version 1.0

- Replacing Kickstart or implementing a bare-metal kernel.
- Parsing raw OFS or FFS structures directly.
- Accessing files while a cartridge is in exclusive runtime mode.
- Supporting OCS/ECS machines, 68000/68010 CPUs, RTG graphics, or non-Amiga hardware.
- Requiring or optimizing primarily for accelerators, Fast RAM, an FPU, or a CD32.
- Full semantic or library compatibility with standard Lua, C, or PICO-8 Lua; MIGA Lua deliberately preserves familiar Lua syntax while changing its type, number, table, allocation, and execution models.
- Generating 68000-compatible game code or supporting A500-class machines; the native compiler targets the stock A1200's 68EC020/68020 instruction set.
- Dynamic linking, user-loadable native plug-ins, hand-written assembly in projects, or externally supplied machine code in cartridges.
- A tracker or sample editor; version 1.0 imports and previews `.mod` files only.
- A general bitmap-paint package, animation package, or full map editor.
- Networking, printing, serial transfer, MIDI, or source control on the Amiga.
- Standalone executable export for individual cartridges.
- NTSC certification for the 256-line display in version 1.0.

## 5. Assumptions and decisions to validate

| ID | Assumption or proposed decision | Rationale | Validation or fallback |
| --- | --- | --- | --- |
| A-01 | PAL A1200 is the certified 1.0 target. | A non-interlaced 256-line workspace naturally fits PAL and provides a stable 50 Hz cadence. | Detect the video standard before opening the workspace. On NTSC, show a clear unsupported-mode message in a safe AmigaOS screen. A 256 × 200 compatibility mode MAY follow later. |
| A-02 | Kickstart/AmigaOS 3.0 and 3.1 are the initial compatibility targets. | These are the normal stock A1200 environments and expose the required classic APIs. | Test both on emulator and original hardware. Newer 3.x releases are best-effort until separately certified. |
| A-03 | MAGI-80 is an AmigaOS-hosted environment, not a bare-metal kernel. | OFS/FFS access through `dos.library`, safe HD launch, and continued AmigaOS operation during editing require a hosted process. | If literal bare-metal operation becomes mandatory, split it into a separate product track with its own loader, filesystem, input, and device work. |
| A-04 | A standard DD ADF is 901,120 bytes raw, conventionally called 880 KiB. | This is the distribution ceiling and matches a stock internal drive. | The release pipeline MUST build and boot-test the exact image rather than relying only on summed file sizes. |
| A-05 | The safe filesystem payload budget is initially 800 KiB. | OFS overhead, boot metadata, and future headroom make the raw ADF size an unsafe payload target. | Generate both OFS and FFS candidate images in Phase 0, select one boot format, and replace this estimate with measured free-block budgets. |
| A-06 | The virtual palette remains 12-bit RGB even though AGA can display 24-bit palette values. | The requested 4,096-color source space is a meaningful fantasy-machine constraint and gives compact assets and simple editing. | Expand each 4-bit component to AGA's 8-bit component when programming the hardware palette. |
| A-07 | MIGA Lua compiles ahead of time directly to native 68020 machine code in RAM. | Strong static types, fixed layouts, direct API calls, and removal of interpreter dispatch maximize the limited stock CPU. | Phase 0 must prove a minimal on-target emitter, cache synchronization, bounded-loop instrumentation, compile time, code size, and safe abort. A bytecode VM would be a fallback redesign, not a parallel 1.0 runtime. |
| A-08 | Baseline simulation is 25 updates per second with 50 Hz video synchronization; 50 updates per second is an optional cartridge mode. | Full dual-layer chunky-to-planar conversion, compiled game logic, and audio must share a stock 68EC020 and Chip RAM. | Phase 0 benchmarks determine whether 50 Hz full-frame games can be promoted to a 1.0 guarantee. Frame rate MUST never change adaptively without the cartridge knowing. |
| A-09 | Runtime file I/O is prohibited. | DOS file calls may wait, which breaks the scheduling freeze and conflicts with direct hardware ownership. | Preload the compiled cartridge and all assets before takeover. Save only after restoration. |
| A-10 | Sprite assets are software-rendered into a logical playfield. | This gives a predictable fantasy API and avoids exposing AGA hardware-sprite limits in the first version. | Hardware sprites MAY later be reserved for the pointer or exposed as an advanced, non-portable API. |
| A-11 | A cartridge is capped initially at 384 KiB uncompressed residency and 256 KiB packed on disk. | This leaves room for the environment, buffers, AmigaOS, and a useful example on one floppy. | Phase 0 and the first vertical-slice game set final caps. The UI MUST display the measured packed and resident sizes. |
| A-12 | One process contains the shell, editors, native compiler, and runtime. | It simplifies boot, disk swapping, generated-code ownership, and restoration. | Split into overlays only if disk or resident-code measurements fail their gates. |
| A-13 | Hosted code uses the C runtime and AmigaOS services instead of duplicating them. | The purpose is to exploit the A1200, not to write another general-purpose OS. | Validate the chosen libc's size, ABI, DOS error behavior, and Kickstart 3.0/3.1 compatibility. Replace only individual facilities whose measured cost or semantics fail the product constraints. |
| A-14 | “Fits on one floppy” is initially interpreted as a self-starting AmigaDOS distribution, not merely a file small enough to copy to floppy. | It provides the most console-like experience and a clean low-memory boot. | If only file-size fit is required, SYS-002 becomes optional and the boot-component licensing gate can be removed; all other disk-size limits remain. |

## 6. Target platform and compatibility contract

### 6.1 Required hardware

- Commodore Amiga 1200 or a cycle-compatible equivalent.
- 68EC020/68020-compatible CPU at the stock clock rate or faster.
- AGA chipset.
- 2 MiB Chip RAM; no other memory is assumed.
- PAL video output for the certified release.
- Internal or compatible Amiga floppy drive for floppy workflows.
- Built-in keyboard.
- At least one standard joystick or mouse port.

### 6.2 Optional hardware

- IDE hard disk or CompactFlash exposed as an AmigaDOS volume.
- Additional Fast RAM.
- Second joystick.
- Accelerator.

Optional hardware MUST NOT be required by a cartridge marked `stock`. MAGI-80 MAY use Fast RAM for hosted-mode buffers or caches, but it MUST preserve the stock-memory execution path and MUST place all DMA-visible data in Chip RAM.

### 6.3 Startup checks

Before opening its workspace, MAGI-80 MUST check:

- CPU capability;
- AGA availability;
- PAL timing;
- required libraries and minimum versions;
- total and largest available Chip RAM blocks;
- whether critical display and audio resources can be acquired;
- whether the launch volume is readable.

Failure MUST return to AmigaOS with a plain explanation. MAGI-80 MUST NOT attempt hardware takeover after a failed preflight.

## 7. Operating model

### 7.1 State model

```text
AmigaOS boot or Workbench/CLI launch
                 |
                 v
      HOSTED INITIALIZATION
                 |
                 v
       HOSTED EDITOR SHELL <---------+
          |             ^            |
          | Run         | Stop/error |
          v             |            |
       PREFLIGHT -------+            |
          | success                  |
          v                          |
    EXCLUSIVE TAKEOVER                |
          |                          |
          v                          |
      CARTRIDGE RUNTIME               |
          |                          |
          v                          |
       RESTORATION ------------------+
```

Every acquisition MUST have a recorded matching release action. Takeover is a transaction: partial failure unwinds only the resources already acquired, in reverse order.

### 7.2 Hosted editor mode

In hosted mode:

- AmigaOS multitasking MUST remain enabled.
- C99 `malloc()`/`free()` and `fopen()`-family functions MAY be used for editor, compiler, and ordinary project I/O, subject to bounded allocations and checked errors.
- Normal disk insertion, validation, and file operations MUST be handled by AmigaDOS.
- The editor SHOULD use a custom Intuition screen or an OS-managed `View` compatible with the 256 × 256 visual design.
- Keyboard and mouse events MUST use OS input facilities.
- Music preview SHOULD use an OS-cooperative audio path and must handle unavailable Paula channels gracefully.
- Direct custom-chip access MUST be limited to operations explicitly coordinated with the appropriate OS resource or library.
- The user MUST be able to leave MAGI-80 and return to Workbench or CLI without rebooting.

### 7.3 Exclusive runtime mode

In exclusive runtime mode:

- The cartridge and all referenced data MUST already be resident.
- All MAGI-80 project/import streams with pending work MUST be flushed and closed before takeover. Inherited CLI/Workbench process handles may remain open but MUST stay untouched until restoration.
- No operation that may call `Wait()` is allowed.
- The MAGI-80 task MUST prevent normal task rescheduling for the duration of the run.
- Hardware interrupts required for MAGI-80 input, video timing, and music MUST continue to operate.
- `Disable()` MUST NOT cover the gameplay session. Official Exec guidance warns that long disabled sections disrupt vital system activity; it may only protect a measured, very short transition when necessary.
- MAGI-80 MUST coordinate ownership of the blitter, display, CIA timer if used, and Paula channels before programming them directly.
- An emergency stop input MUST remain available even when generated user code loops forever. The compiler MUST insert stop and execution-budget checks at every backward control-flow edge and other bounded safe points.
- A generated-code fault detected by a guard, execution-budget exhaustion, or user stop MUST enter the same restoration path.
- The runtime MUST perform no dynamic memory allocation after takeover.

### 7.4 Proposed takeover sequence

The exact register and library sequence is a Phase 0 deliverable, not something to improvise late in development. The initial design is:

1. Compile the cartridge and validate all resources.
2. Validate generated-code bounds, relocations, guarded control-flow metadata, stack requirements, and calls against the immutable MAGI-80 native ABI jump table.
3. Flush the generated code range from the 68020 instruction cache with the appropriate Exec cache-control function before it can execute.
4. Allocate and pin all remaining runtime memory.
5. Finish, flush, and close MAGI-80 project/import file operations; stop hosted audio preview.
6. Save the active `View`, display configuration, palette, DMA/interrupt state that MAGI-80 will modify, and input/audio ownership state.
7. Install or activate preallocated MAGI-80 interrupt handlers through the appropriate Exec/CIA resource interfaces.
8. Obtain exclusive blitter access and wait for any prior blit to finish.
9. Blank or detach the OS view using `graphics.library`, with required frame waits completed before scheduling is forbidden.
10. Call `Forbid()` once the runtime path is guaranteed not to wait.
11. In a short critical transition, install the MAGI-80 Copper list, bitplane pointers, palette, DMA, and owned interrupt sources.
12. Run the fixed-step native callback loop, synchronized by an interrupt-set flag or a nonblocking beam/tick mechanism.

Restoration reverses those steps: stop MAGI-80 DMA and interrupt sources, restore the saved display and owned resources, release the blitter, call `Permit()`, restart hosted input/audio as needed, and redraw the editor. The implementation MUST track nesting and ownership explicitly; it MUST never issue an unmatched `Permit()`, `Enable()`, `DisownBlitter()`, or resource release.

### 7.5 Restoration acceptance test

A stock A1200 MUST survive at least 1,000 automated or operator-assisted run/stop cycles in one session while alternating graphics, input, and music test cartridges. After each stop:

- the editor display is intact;
- keyboard and mouse input work;
- OS task scheduling resumes;
- the system clock continues sensibly;
- disks and hard-disk volumes remain usable;
- no Chip RAM or OS resource leak is detected;
- no stale audio DMA or stuck note remains;
- the prior Workbench/CLI state remains usable after MAGI-80 exits.

## 8. User experience

### 8.1 Main shell

The main shell MUST expose these workspaces without requiring AmigaOS UI interaction:

- **Code**
- **Sprites**
- **Music**
- **Files / Cartridges**
- **Run**
- **Help / System**

The shell SHOULD use consistent function-key shortcuts and show them on screen. Exact bindings are configurable during usability testing, but there MUST always be dedicated actions for run, stop, save, switch editor, and help.

### 8.2 Visual workspace

The MAGI-80 UI SHOULD use the same 256 × 256 logical surface as cartridges so that asset previews are exact. A compact 6 × 8 or similarly legible bitmap font SHOULD provide at least 40 source columns. The code editor MUST support horizontal scrolling because the logical screen is intentionally narrow.

### 8.3 Immediate feedback

- A successful compile SHOULD start the cartridge within two seconds when no disk access is needed.
- Compile diagnostics MUST return focus to the first error.
- Stopping a cartridge MUST return to the prior editor and cursor location.
- Runtime errors MUST report the source file, line, function, and concise cause whenever debug metadata is present.
- Memory, source, generated-code, graphics, music, and packed-cartridge budgets MUST be visible from the shell.

## 9. Functional requirements

### 9.1 Boot, launch, and shutdown

| ID | Requirement |
| --- | --- |
| SYS-001 | The full release image MUST fit on one standard 880 KiB Amiga DD floppy. |
| SYS-002 | The floppy edition MUST boot through a minimal AmigaDOS startup sequence and enter MAGI-80 without opening Workbench. |
| SYS-003 | The same MAGI-80 core executable MUST be launchable from CLI and Workbench on a hard disk. |
| SYS-004 | After a floppy boot completes, the executable and all mandatory UI resources MUST be resident so that the boot disk can be exchanged for a project disk. |
| SYS-005 | Quit MUST restore the original AmigaOS display, input, audio, scheduling, current directory, and error status as far as the public APIs permit. |
| SYS-006 | A failed startup MUST release every resource acquired by that startup. |
| SYS-007 | The release MUST include a version screen containing build ID, cartridge format version, MIGA Lua language version, native ABI version, compiler version, and target profile. |

### 9.2 Code editor

| ID | Requirement |
| --- | --- |
| CODE-001 | The editor MUST create, open, edit, save, and save-as MIGA Lua source stored in a cartridge project. |
| CODE-002 | It MUST support cursor movement, selection, insert/delete, line operations, page movement, and configurable two- or four-column tab expansion. |
| CODE-003 | It MUST provide bounded undo/redo with a visible remaining history budget. |
| CODE-004 | It MUST provide find, find-next, go-to-line, and matching-delimiter navigation. |
| CODE-005 | It MUST display line and column numbers and indicate modified state. |
| CODE-006 | It SHOULD provide token coloring for comments, keywords, literals, identifiers, and diagnostics. |
| CODE-007 | Compiler errors MUST be selectable and navigate to their source span. |
| CODE-008 | Text MUST use a documented single-byte encoding. Version 1.0 SHOULD use printable ASCII plus tab and newline, with LF canonicalized internally. |
| CODE-009 | The editor MUST handle a source file up to the cartridge source limit without an unbounded allocation or quadratic whole-file operation on every keystroke. |
| CODE-010 | Source changes MUST never be written automatically to floppy without an explicit user preference and visible write indication. |

### 9.3 Sprite editor

| ID | Requirement |
| --- | --- |
| SPR-001 | The editor MUST edit indexed 4-bit sprite pixels using either the background or foreground logical palette. |
| SPR-002 | It MUST provide pencil, eraser/transparent color, fill, line, rectangle, selection, move, copy, and paste. |
| SPR-003 | It MUST provide horizontal and vertical flip and 90-degree rotation for square selections. |
| SPR-004 | It MUST support at least 8 × 8, 16 × 16, and 32 × 32 logical sprite cells. |
| SPR-005 | It MUST show a zoomed editing grid and a 1:1 preview on both light and dark checker backgrounds. |
| SPR-006 | It MUST edit all 31 opaque logical palette entries as 12-bit RGB values and clearly mark foreground index 0 as transparent. |
| SPR-007 | Palette edits MUST update previews immediately and be undoable. |
| SPR-008 | Sprite sheets MUST use the same packed asset representation consumed by the runtime or a lossless build-time transformation of it. |
| SPR-009 | The editor MUST prevent a background/foreground palette mismatch from silently changing pixel indices. |
| SPR-010 | Hardware-sprite authoring is not required for 1.0. The term `sprite` in the public API means a software sprite blitted into a virtual playfield. |

### 9.4 ProTracker import and playback

| ID | Requirement |
| --- | --- |
| AUD-001 | MAGI-80 MUST import standard four-channel, 31-sample ProTracker modules with recognized signatures such as `M.K.`, `M!K!`, and `4CHN`. |
| AUD-002 | Import MUST validate the header, song length, order table, pattern count, sample lengths, loop ranges, and total file bounds before allocating or copying bulk data. |
| AUD-003 | Unsupported signatures, effects, corrupt loops, truncated patterns, and over-budget samples MUST produce specific errors and MUST NOT destabilize the editor. |
| AUD-004 | The importer MUST preserve signed 8-bit sample data, finetune, volume, loop points, pattern data, and song order for supported files. |
| AUD-005 | The player MUST use Paula's four DMA audio channels in exclusive runtime mode; samples MUST reside in Chip RAM. |
| AUD-006 | The replay engine MUST implement the ProTracker 2.x effect subset declared by the compatibility test suite. The required 1.0 subset is effects `0`, `1`, `2`, `3`, `4`, `5`, `6`, `9`, `A`, `B`, `C`, `D`, `E1`, `E2`, `E6`, `E9`, `EA`, `EB`, `EC`, `ED`, `EE`, and `F`. |
| AUD-007 | Effects `7`, `8`, `E0`, `E3`, `E4`, `E5`, `E7`, and nonstandard variants SHOULD be implemented when behavior is verified; otherwise import MUST warn and list occurrences. |
| AUD-008 | Both speed and CIA-tempo interpretations of `Fxx` MUST match the selected compatibility reference within the limits of PAL hardware timing. |
| AUD-009 | Hosted-mode preview MUST coexist with AmigaOS through allocated audio/timer resources or report that preview is unavailable. It MUST NOT take exclusive runtime ownership merely to browse a module. |
| AUD-010 | Starting a game MUST stop hosted preview, acquire required resources, and start replay from a deterministic state. |
| AUD-011 | The API MUST provide at least `music_play`, `music_stop`, `music_position`, and per-channel mute for debugging. |
| AUD-012 | Version 1.0 does not need to edit or export `.mod` files. |

### 9.5 Storage and filesystem behavior

| ID | Requirement |
| --- | --- |
| IO-001 | In hosted mode, MAGI-80 MUST enumerate mounted volumes and directories through `dos.library`. |
| IO-002 | It MUST read files on OFS and FFS volumes as exposed by AmigaDOS; it MUST NOT depend on direct knowledge of their on-disk block formats. |
| IO-003 | It MUST open, read, seek when supported, examine, enumerate, create, write, flush, close, rename, and delete through documented AmigaOS facilities or the selected C99 runtime backed by them. Volume enumeration, disk-specific state, and detailed DOS errors MAY use `dos.library` directly; ordinary sequential file data MAY use `fopen()` and related calls. |
| IO-004 | File browsing MUST support `DF0:` and normal hard-disk volume/device names. |
| IO-005 | MAGI-80 MUST detect disk removal, insertion, validation delays, write protection, full media, name collisions, and DOS errors without losing the in-memory project. |
| IO-006 | Saves SHOULD use a temporary sibling file, flush and close it, then replace the destination only after success. If the volume lacks room for both copies, the UI MUST explain the risk and offer Save As to another volume; it MUST NOT silently overwrite the only valid copy. |
| IO-007 | No MAGI-80-owned project/import handle, lock, outstanding DOS packet, or unflushed project write may cross into exclusive runtime mode. Inherited process handles may remain idle but MUST NOT be accessed while task scheduling is frozen. |
| IO-008 | File and volume names MUST be normalized conservatively and MUST remain usable on both OFS and FFS. |
| IO-009 | Unknown files are read-only imports until their parser validates them. |
| IO-010 | A cartridge loaded from hard disk MUST behave identically to the same bytes loaded from floppy. |

### 9.6 Cartridge workflow

| ID | Requirement |
| --- | --- |
| CART-001 | A project MUST contain metadata, MIGA Lua source, palettes, sprites, optional map data, and optional ProTracker music. Native code and source line mappings are generated in RAM by the trusted compiler. |
| CART-002 | A shareable cartridge SHOULD be a single file with magic, format version, section directory, declared lengths, checksums, and no native pointers. |
| CART-003 | Multi-byte fields MUST use a documented byte order; big-endian is preferred to minimize target conversion. |
| CART-004 | Every section MUST be independently bounds-checked before use. Unknown optional sections MUST be skippable. Unknown mandatory sections MUST reject the cartridge. |
| CART-005 | Version 1.0 cartridges MUST NOT embed executable machine code. MAGI-80 MUST compile source into a fresh native-code arena for the current compiler and ABI, preventing stale or externally injected native code from bypassing language safety. |
| CART-006 | A release cartridge MUST be reproducible from its source and assets. Given the same MIGA Lua compiler, native ABI, and target profile, compilation MUST produce the same generated code and data layout. |
| CART-007 | Compression MUST be deterministic, streamable or bounded-memory, and fast enough to load from floppy. RLE and a small LZ-family codec are candidates. |
| CART-008 | The shell MUST show packed disk size and worst-case resident size before saving. |
| CART-009 | A malformed cartridge MUST never reach native code generation with an unchecked section, offset, count, or decompression result. |
| CART-010 | The cartridge MUST identify its container, MIGA Lua language, and target-profile versions. Compiler and native ABI versions belong to the MAGI-80 system and trusted in-session compiled image, not to an executable cartridge payload. |

## 10. MIGA Lua language and native compiler

### 10.1 Design intent and compatibility promise

The built-in language, provisionally named **MIGA Lua**, is a strongly and statically typed game language whose surface syntax stays as close as practical to Lua 5.1. Familiarity and easy transfer of programming habits are goals; full Lua semantics, standard-library compatibility, and the ability to run arbitrary `.lua` programs are not.

MIGA Lua deliberately replaces Lua's dynamically typed values, universal tables, floating-point default, garbage-collected heap, dynamic loading, and metaprogramming with fixed layouts that compile efficiently for a 14 MHz 68EC020. Unsupported Lua constructs MUST be rejected at compile time with a specific diagnostic rather than accepted with subtly different runtime behavior.

The native compilation pipeline is:

```text
MIGA Lua source
      -> lexer and Lua-like parser
      -> typed AST and whole-program checks
      -> compact typed IR
      -> data and stack layout
      -> 68020 instruction selection and register allocation
      -> machine-code emission and relocation
      -> generated-code validation
      -> Exec instruction-cache synchronization
      -> bounded native execution
```

The compiler MUST run on the stock A1200 and in host-side tests. It writes 68020 instruction words directly into a preallocated code arena; it does not invoke GCC, an assembler, or a linker on the Amiga. It SHOULD use bounded passes, reusable arenas, and simple predictable optimizations. A failed compilation MUST leave no executable partial result.

### 10.2 Proposed source style

```lua
type Player = {
  x: fix,
  y: fix,
  tile: u8
}

const SPEED: fix = 2.0

local player: Player = {
  x = 128.0,
  y = 128.0,
  tile = 0
}

function init(): void
  music_play(0)
end

function update(): void
  if btn(LEFT) then
    player.x = player.x - SPEED
  elseif btn(RIGHT) then
    player.x = player.x + SPEED
  end
end

function draw(): void
  clear(BACK, 0)
  clear(FRONT, 0)
  sprite(player.tile, player.x, player.y, FRONT)
end
```

Type annotations are mandatory at public boundaries and when inference would be ambiguous. Local declarations and record literals SHOULD normally infer their types so that common code remains visually close to Lua. Phase 3 MUST freeze a grammar and a Lua-compatibility matrix before implementing the production parser.

### 10.3 Lua-like syntax contract

Version 1.0 MUST support, where compatible with the static type model:

- `function`, `local`, `if`/`then`/`elseif`/`else`/`end`, `while`, `repeat`/`until`, and numeric `for` syntax;
- generic `for` over compiler-known arrays and dictionaries;
- Lua-style function calls, field access, indexing, table constructors, comments, and lexical conventions;
- Lua operator spelling and precedence, with short-circuit `and`/`or` on `bool` values;
- boolean conditions; optional values require an explicit presence or `nil` comparison rather than implicit coercion;
- multiple assignment and statically typed multiple returns;
- the `:` method-call spelling when the receiver and target function resolve statically;
- immutable string literals and compile-time concatenation;
- source line and column mappings for every generated safe point and diagnostic;
- MIGA-specific `type`, `const`, type annotation, capacity, and fixed-size generic syntax.

MIGA Lua source SHOULD look familiar to a Lua programmer, but the documentation MUST call it a dialect and list every material difference from Lua 5.1.

### 10.4 Strong type and number model

The required built-in types are:

- `bool`;
- `i8`, `u8`, `i16`, `u16`, `i32`, and `u32`;
- `fix`, provisionally a signed 16.16 fixed-point value;
- `color`, `layer`, `button`, `sprite_id`, and similar small domain types where they prevent accidental API misuse;
- immutable, interned `string` or `symbol` values;
- `T?` optional values with explicit absence;
- fixed-layout records;
- `array<T, N>`;
- `dict<K, V, N>`;
- statically known function signatures and multiple-return tuples.

There is no implicit `any` or universal tagged-value type in the stock profile. Numeric literals are typed from context. Narrowing, signed/unsigned changes, and `i32`/`fix` conversion require an explicit conversion unless proven lossless. No source operation may cause the compiler or runtime to link software floating-point support.

Top-level `local` and `const` declarations have module-static storage known at compile time and may be referenced by top-level functions, as in the example above. They do not allocate closure environments. Nested functions that capture activation-local values remain excluded in version 1.0.

Ordinary integer arithmetic SHOULD use defined two's-complement wrapping; checked conversion helpers MUST be available. Fixed-point multiplication, division, trigonometry, and rounding semantics MUST be bit-exact across host tests and the 68020 backend.

### 10.5 Records, arrays, and optimized dictionaries

Lua table-constructor syntax maps to one of three statically selected layouts:

1. **Record:** a constructor with named fields and a fixed inferred or declared shape becomes a compact record. Field access compiles to a constant byte displacement; it performs no hash lookup.
2. **Array:** a homogeneous sequence becomes contiguous `array<T, N>` storage. Capacity and element size are compile-time constants, and each dynamic index is bounds-checked.
3. **Dictionary:** a declared `dict<K, V, N>` becomes a fixed-capacity typed hash table. It is used only when keys are genuinely dynamic.

The 1.0 dictionary contract is:

- capacity `N` is fixed at compile time and storage is allocated before takeover;
- supported keys are bounded integers, enums/symbols, and interned immutable strings; arbitrary runtime strings are excluded initially;
- interned string literals are converted to stable numeric symbols before execution, avoiding runtime string hashing for common asset/state keys;
- capacity SHOULD be a power of two so bucket selection avoids division;
- the implementation SHOULD use deterministic open addressing with linear or Robin Hood probing and a documented maximum load factor;
- deletion uses a bounded documented tombstone or backward-shift strategy;
- insertion into a full dictionary produces a controlled source-level runtime fault;
- iteration order is deterministic and defined by the implementation, but need not match insertion order;
- record syntax MUST NOT silently degrade to a dictionary because of one misspelled field.

There is no general garbage-collected table type. Dynamic game collections use fixed arrays, typed dictionaries, or later a separately specified fixed-capacity pool type.

### 10.6 Excluded or restricted language features

- binary floating point and the standard Lua `number` model;
- unrestricted type changes and implicit dynamic dispatch;
- an unbounded heap or general garbage collector;
- runtime source compilation, `load`, `loadstring`, `dofile`, or `require`;
- `io`, `os`, `package`, `debug`, and other host-operating-system libraries;
- metatables, metamethods, weak tables, finalizers, reflection, or `eval`;
- coroutines or threads;
- userdata, pointers, arbitrary memory access, native-code literals, and inline assembly;
- captured closures in version 1.0; non-capturing function values MAY be supported when their targets resolve statically;
- unrestricted varargs;
- exceptions and user-defined trap handlers;
- recursion by default; if later enabled per function, it MUST consume a declared bounded stack and retain stop/budget checks;
- allocation of code or variable-sized objects during exclusive execution.

### 10.7 Program lifecycle

A cartridge MAY define these statically resolved entry points:

- `init(): void` — called once after the runtime and preallocated data are ready;
- `update(): void` — called at the cartridge's declared fixed rate;
- `draw(): void` — called after an update when a new frame is requested;
- `shutdown(): void` — called on a normal stop under a strict execution budget.

Missing callbacks are legal no-ops. MAGI-80, not generated code, owns the main loop, frame pacing, interrupt handling, error trampoline, and emergency stop path. `init()` may populate preallocated arrays and dictionaries but may not grow their capacity or allocate memory.

### 10.8 Native 68020 backend and ABI

The compiler targets the 68EC020/68020 user-mode instruction set directly. It MUST NOT emit FPU, MMU, 68030+, or privileged instructions. Literal 68000 compatibility is not a goal because the certified AGA platform already supplies a 68EC020.

The backend SHOULD initially implement only optimizations with clear value and small code cost:

- constant folding and propagation;
- removal of unreachable and trivially dead code;
- direct register or fixed stack-slot allocation for locals;
- constant-offset record fields;
- scaled or strength-reduced fixed-array addressing;
- inlining of very small arithmetic and guard helpers when it reduces total cost;
- direct native calls for recognized MAGI-80 built-ins;
- shared assembly helpers for expensive fixed-point operations.

The native ABI MUST define:

- one reserved address register for an immutable runtime-context pointer or an equivalent measured convention;
- caller/callee-saved data and address registers;
- a dedicated, fixed-size generated-code stack, with the hosted process stack saved outside it by a reviewed assembly entry trampoline;
- stack alignment, statically computed call-graph depth, maximum frame size, and overflow guard strategy;
- argument and multiple-return placement;
- an immutable, versioned jump table containing the only runtime functions callable by generated code;
- typed signatures and stable numeric IDs for every fantasy API function;
- error and stop trampolines that return control to MAGI-80 without returning through an invalid user stack;
- relocation kinds, code alignment, code-arena bounds, and source-map metadata.

Generated code MUST NOT address AmigaOS libraries, custom-chip registers, the Copper list, unrelated MAGI-80 state, or arbitrary absolute memory. Hardware access remains inside reviewed C/assembly runtime functions reached through the native ABI.

### 10.9 Native-code safety, interruption, and determinism

Native generation removes interpreter overhead but also removes the structural safety of a bytecode dispatch loop. A stock 68EC020 provides no process isolation suitable for this design, so the compiler, emitter, guards, and ABI become part of the trusted computing base.

The following controls are mandatory:

- every array access, dynamic sprite/map index, dictionary operation, division, shift, conversion, and API argument with a safety range MUST be guarded unless the compiler proves it safe;
- every backward control-flow edge MUST decrement an execution budget and test the asynchronous stop flag;
- the compiler MUST reject recursive call graphs in version 1.0 and compute a worst-case native stack requirement before execution;
- function entry and other bounded safe points MUST test stack and stop state as required by the call graph;
- finite straight-line code is bounded by the maximum source/generated-code size;
- budget exhaustion, dictionary-full, divide-by-zero, invalid shift, bounds failure, stack failure, and invalid API input branch to a runtime error trampoline carrying a source location;
- generated branch targets, relocations, code/data ranges, entry points, stack requirements, guard metadata, and jump-table call targets MUST be validated before execution;
- release tests MUST disassemble or otherwise independently check emitted instruction streams on the host; malformed IR and relocation fuzzing MUST never produce an installable code image;
- generated code MUST reside in a dedicated fixed-size arena and MUST never be loaded directly from an untrusted cartridge section;
- after emission and relocation, MAGI-80 MUST call the appropriate Exec cache-clear function for the generated range before execution, because the 68020 has an instruction cache;
- no generated code or game data allocation occurs after takeover;
- random number generation uses a documented algorithm and explicit seed;
- given the same source, compiler version, assets, seed, and input sequence, game-visible state MUST be reproducible.

A compiler or native runtime defect can still corrupt the process and prevent restoration because there is no hardware sandbox. This residual risk MUST be documented, minimized through differential tests and fuzzing, and treated as more severe than an ordinary cartridge error.

### 10.10 Minimum fantasy API

The 1.0 API MUST cover:

- lifecycle and timing: `frame`, `ticks`, and cartridge rate metadata;
- input: `btn`, `btn_pressed`, keyboard key state where supported;
- graphics state: `layer`, `camera`, `clip`, `palette`, `transparent`;
- pixels and primitives: `clear`, `pixel`, `line`, `rect`, `rect_fill`, `circle`, `circle_fill`;
- assets: `sprite`, `sprite_region`, `map_draw` if map data ships in 1.0;
- text: `print` with the built-in font;
- math: integer, fixed-point, trigonometric lookup, clamp, min/max, and deterministic random;
- audio: module play/stop/position and debug mute controls;
- diagnostics: bounded `trace` captured for display after restoration.

APIs MUST use logical coordinates and typed handles. They must not expose Chip RAM addresses, bitplane layouts, Copper instructions, AmigaOS handles, native function pointers, or arbitrary callbacks.

## 11. Graphics specification

### 11.1 Logical model

- Resolution: exactly 256 × 256 logical pixels.
- Layers: `BACK` and `FRONT`, independently addressable by the API.
- Per-layer depth: 4 bits per pixel.
- `BACK`: 16 visible colors numbered 0–15.
- `FRONT`: index 0 is transparent; indices 1–15 are visible.
- Palette: 31 opaque colors, each selected from 12-bit RGB (`0xRGB`, four bits per component).
- Composition: `FRONT` over `BACK`, with no blending or partial alpha.
- Coordinates: integer, origin at top left.
- Out-of-bounds drawing: clipped, never wrapped unless an individual API explicitly requests wrapping.

### 11.2 Candidate runtime representations

The first implemented representation uses one combined byte per screen coordinate:

- low nibble: `BACK` index;
- high nibble: `FRONT` index;
- total logical framebuffer: 65,536 bytes.

This preserves two logical 4-bit layers in a compact buffer and permits a single 8-bit chunky-to-planar pass. Drawing into one layer uses a masked byte update.

Phase 0 now compares four internal candidates:

| Candidate | Source storage | Purpose |
|---|---:|---|
| Combined `fb8` | 64 KiB | Compact conventional eight-plane C2P input |
| Two packed 4-bit layers | 64 KiB | Compact storage with independent playfield conversion |
| Two byte-per-pixel 4-bit layers | 128 KiB | Direct per-layer byte stores at the cost of extra read bandwidth |
| Byte FRONT plus packed BACK | 96 KiB | Fast dynamic-foreground writes with a compact background |

The representation is not part of the cartridge or language ABI and MUST NOT be exposed as a raw address. Packed cartridge assets, runtime drawing surfaces, planar sprite caches, and display buffers MAY use different representations. The runtime layout remains open until the optimized real-A1200 benchmark gate described in [Chunky Layout and C2P Benchmark](./c2p-layout-benchmark.md).

### 11.3 AGA mapping

The display uses eight low-resolution AGA bitplanes in dual-playfield mode:

- odd-numbered hardware bitplanes form one 4-bit playfield;
- even-numbered hardware bitplanes form the other;
- playfield priority places `FRONT` above `BACK`;
- the foreground zero combination is transparent;
- the rear zero combination reveals the global backdrop color, providing background logical color 0;
- AGA `BPLCON3.PF2OF` selects a non-overlapping 16-entry color-bank offset for the second playfield;
- hardware color 0 represents `BACK[0]`, colors 1–15 represent the 15 opaque `FRONT` entries, and the selected second bank represents `BACK[1..15]`; the unused transparent bank entry is ignored.

The exact PF1/PF2 assignment, palette-register mapping, priority bits, fetch mode, modulo, and plane-pointer ordering MUST be captured in a hardware test and then frozen in a register-level design note.

The first hosted Phase 0 smoke test provisionally maps `FRONT` to PF1 (BPL1/3/5/7, palette base 0) and `BACK` to PF2 (BPL2/4/6/8, palette base 16). An Intuition-managed PAL 256 × 256 × 8 dual-playfield screen passes mode, palette, Chip-RAM, reference-C2P output, raster-pattern, and repeated restoration checks under FS-UAE/Kickstart 3.0. The C99 converter also passes byte-exact golden vectors natively on macOS. This is not yet the hardware freeze: direct register/Copper mapping, optimized C2P timing, Kickstart 3.1, and real-A1200 gates remain open. See [Hosted AGA Screen Smoke Test](./aga-screen-smoke.md) and [Reference Chunky-to-Planar Converter](./c2p-reference.md).

Although AGA palette entries accept more precision, MAGI-80 expands each virtual 4-bit component to an 8-bit hardware component by nibble replication. Copper palette changes are not part of the base cartridge API; this keeps the 31-color limit stable.

### 11.4 Conversion and buffering

- Conversion MUST be transparent to user code.
- The reference C converter MUST produce bit-exact output for every pixel and plane.
- An optimized 68020 assembly converter is expected and must be tested against the C reference.
- Display memory MUST be in Chip RAM and aligned for the selected AGA fetch mode.
- Two 64 KiB planar frame sets require 128 KiB. Depending on the source layout, framebuffer storage totals 192, 224, or 256 KiB before converter scratch space.
- Plane pointers MUST swap only at a safe display boundary.
- The converter MAY process dirty tiles, but a correct full-frame path is mandatory.
- A missed conversion deadline MUST repeat the prior frame rather than expose partially converted planes.

The allocation-free C99 reference implementations are now executable both as native macOS correctness tests and as target code writing the eight real planes of the hosted AGA screen. All four source layouts produce byte-identical plane checksums. They define authoritative semantics and plane ordering; they do not satisfy a performance target.

Performance decisions MUST use the combined cost of source construction, representative pixel and primitive rendering, C2P, safe display publication, and audio coexistence. They MUST compare CPU-only assembly, a CPU/blitter hybrid, and planar-native high-level primitives. Isolated scalar-C timings MUST NOT freeze the source layout.

### 11.5 Video timing and performance targets

- Certified timing: PAL, 50 Hz display refresh.
- Default simulation rate: 25 Hz, with each completed frame shown for two refreshes.
- Optional simulation rate: 50 Hz for cartridges that fit the measured CPU budget.
- Input-to-display latency: no more than two display refreshes in the standard path.
- Empty runtime: no missed video deadlines over a 30-minute test.
- Representative 25 Hz game: no torn frames and no audio starvation over a 30-minute test.
- The Phase 0 converter spike MUST report full-frame, single-layer-dirty, and no-change costs on real 2 MiB Chip-RAM hardware, with bitplane DMA enabled.

### 11.6 Editor and runtime drawing

The editor and generated-code runtime MUST share the same clipping, palette, primitive, font, and sprite semantics. A portable C renderer is authoritative. Generated code reaches it through the native ABI; optimized paths may replace individual operations only when golden-image tests remain identical.

## 12. Input specification

### 12.1 Hosted input

Hosted mode MUST use AmigaOS input facilities and support:

- built-in keyboard;
- mouse movement and buttons;
- joystick port 1;
- joystick port 2 when present.

### 12.2 Runtime input

Runtime MUST support at least:

- four directions and one action button from a standard Amiga joystick, plus a second button when the connected controller and chosen input method support it;
- a defined subset of keyboard keys;
- a stop chord that generated code cannot mask or redefine;
- per-frame current, pressed, and released states.

The implementation may use preinstalled OS interrupt/resource mechanisms or carefully owned hardware access, but it MUST not depend on an AmigaOS task being scheduled during exclusive mode. Keyboard acknowledgement and CIA ownership are high-risk items and MUST be proven in Phase 0 on real hardware.

Input events used by the cartridge MUST be sampled at deterministic frame boundaries. The interrupt/input backend owns the emergency-stop flag, and compiler-inserted safe points MUST observe it independently of cartridge logic.

## 13. Audio architecture

### 13.1 Hosted backend

The hosted backend owns no hardware without allocation. It uses `audio.device` and the appropriate CIA resource or a similarly cooperative mechanism. If another application holds the channels or timer, MAGI-80 SHOULD continue editing silently and explain why preview is disabled.

### 13.2 Exclusive backend

Before takeover, the runtime reserves the four Paula channels and any required timer. During execution:

- Paula reads samples directly from Chip RAM;
- the replayer updates periods, volumes, sample addresses, and lengths from an interrupt-safe state;
- generated code never writes Paula registers directly;
- tempo processing must not allocate, call DOS, or wait;
- stopping resets DMA and volumes before OS audio ownership is returned.

CIA-timed replay is preferred for correct ProTracker tempo. A VBlank-based fractional scheduler MAY be used initially if its compatibility and jitter are measured, documented, and accepted at a roadmap gate.

### 13.3 Compatibility corpus

The project MUST maintain a legal, redistributable test corpus containing:

- one minimal module for each required effect;
- combined-effect and effect-memory cases;
- loop edge cases;
- maximum legal pattern/order cases;
- truncated and malicious files;
- at least three representative songs cleared for inclusion or test use.

Expected row, tick, period, volume, loop, and song-position traces SHOULD be compared with a declared ProTracker compatibility reference. Audio waveform equivalence is useful but state-trace equivalence is the primary deterministic test.

## 14. Storage and packaging

### 14.1 Floppy edition

The release build MUST produce a bootable `.adf` and a manifest. The image SHOULD contain only:

- boot metadata and `S:Startup-Sequence`;
- the MAGI-80 executable;
- embedded or separate mandatory fonts/help data;
- one small example cartridge;
- license and short read-me files if space permits.

Initial payload allocation:

| Component | Target ceiling |
| --- | ---: |
| Boot glue and required system files | 48 KiB |
| Packed MAGI-80 executable and mandatory data | 440 KiB |
| Built-in help, font, and templates | 64 KiB |
| Example cartridge | 192 KiB |
| Filesystem and growth reserve | 56 KiB |
| **Total planned payload** | **800 KiB** |

The release pipeline MUST fail when the actual image exceeds its block budget. Compression MAY be used, but the decompressor becomes target code and must obey the C99/limited-assembly policy and restoration requirements.

### 14.2 AmigaOS licensing constraint

A bootable AmigaDOS disk may require copyrighted operating-system components or files not owned by the MAGI-80 project. The project MUST NOT redistribute them without a valid license.

At least one legally viable release route is required:

1. distribute an installer that builds the bootable image from the user's licensed AmigaOS media;
2. obtain redistribution permission;
3. use a compatible redistributable component after proving it meets the stock A1200 requirements; or
4. distribute the MAGI-80 files as a non-bootable disk plus instructions, while treating the bootable-image requirement as not yet complete.

This legal decision is a release gate, not a documentation footnote.

### 14.3 Hard-disk edition

- Installation MUST work by copying one directory to an AmigaDOS volume.
- The main binary MUST launch from CLI.
- A Workbench tool icon and tooltypes SHOULD be provided.
- Paths MUST be relative to the program or use an assigned MAGI-80 volume name; hard-coded `DH0:` paths are prohibited.
- The hard-disk edition MAY contain more examples and offline documentation, but the core behavior and cartridge limits MUST match the floppy edition.

### 14.4 Suggested cartridge container

The proposed `.m80` layout is:

```text
Header
  magic = "M80C"
  container version
  MIGA Lua language version
  target profile
  flags
  section count
  total packed and resident sizes
  whole-file checksum

Section directory[]
  type
  flags (mandatory, compressed)
  offset
  packed length
  resident length
  checksum

Sections
  META  cartridge metadata and limits
  SRC   canonical MIGA Lua source
  PAL   31 virtual 12-bit colors
  GFX   sprite-sheet data
  MAP   optional map data
  MOD   optional validated module
  NOTE  optional author notes
```

No loader may trust header counts, sizes, offsets, compression output sizes, or checksums before bounds validation.

## 15. Memory and performance budgets

### 15.1 Memory policy

All RAM on a stock A1200 is Chip RAM and is shared with DMA. MAGI-80 MUST behave correctly without Fast RAM and MUST account for contention, not merely capacity.

Initial peak target:

| Area | Target ceiling |
| --- | ---: |
| Program code, read-only data, globals, and C runtime | 300 KiB |
| Chunky and double-buffered planar video | 192 KiB |
| Loaded cartridge, including module samples | 384 KiB |
| Editor text, undo, compiler arenas, and diagnostics | 256 KiB |
| Generated native code, globals, guarded stack, dictionaries, and runtime work memory | 128 KiB |
| Copper, audio state, input queues, OS objects, alignment, and reserve | 128 KiB |
| **MAGI-80 target peak** | **1,388 KiB** |
| **Nominal remainder for AmigaOS and safety** | **660 KiB** |

These are ceilings, not allocations that must all be permanent. Editor/compiler arenas SHOULD be reset and reused for runtime work. The executable MUST avoid a single large contiguous allocation when smaller pools are sufficient. Preflight MUST report both total free memory and largest free block.

### 15.2 Allocation rules

- Every subsystem MUST have an explicit arena or owner.
- Hosted subsystems MAY use `malloc()`/`free()` where the ownership and ceiling remain explicit; a custom general-purpose allocator is not required.
- Runtime allocations MUST finish before takeover.
- Importers MUST validate declared lengths before allocation.
- Undo history and diagnostic logs MUST be bounded.
- Temporary compiler structures SHOULD be arena-allocated and released in bulk.
- DMA-visible buffers and samples MUST use Chip RAM allocation flags.
- If Fast RAM exists, only non-DMA hosted data MAY prefer it; success on Fast RAM MUST not conceal a stock-memory regression.
- Allocation failure MUST preserve the current project and return a useful size report.

### 15.3 CPU budgets

Phase 0 must establish measured budgets rather than estimate them from emulator speed. The frame profiler MUST separately measure:

- generated native `update()` logic;
- generated native `draw()` logic and fantasy-API calls;
- sprite and primitive rendering;
- chunky-to-planar conversion;
- Copper/frame synchronization;
- ProTracker tick processing;
- input handling;
- editor redraw and compiler time.

The runtime SHOULD expose an optional raster-bar or numeric profiler. A representative stock cartridge at 25 Hz MUST retain at least 20% measured CPU headroom after audio and conversion. Optional 50 Hz mode must pass the same test with its declared content.

## 16. Software architecture

### 16.1 Major components

| Component | Responsibility |
| --- | --- |
| `shell` | Workspace navigation, commands, help, status, and error presentation. |
| `editor_code` | Text buffer, rendering, commands, undo, and diagnostic navigation. |
| `editor_sprite` | Pixel tools, palette tools, selection, previews, and undo. |
| `compiler_frontend` | Lua-like lexer/parser, type inference, semantics, typed AST/IR, and source maps. |
| `codegen_68020` | Layout, instruction selection, register/stack allocation, machine-code emission, relocation, validation metadata, and cache synchronization. |
| `native_runtime` | Versioned jump table, guarded callback invocation, execution budgets, stop/error trampolines, and fantasy API dispatch. |
| `cartridge` | Container validation, packing/unpacking, versioning, and size accounting. |
| `storage` | Hosted C stream and AmigaDOS volume, directory, error, and safe-save operations. |
| `video_core` | Logical framebuffer, primitives, sprite rendering, palette, and clipping. |
| `video_hosted` | OS-cooperative editor display. |
| `video_exclusive` | Copper, bitplanes, C2P, frame swap, and hardware state restoration. |
| `audio_mod` | Validated MOD model, tick/effect state machine, and trace tests. |
| `audio_hosted` | OS-cooperative preview. |
| `audio_exclusive` | Paula/CIA ownership and register backend. |
| `input_hosted` | AmigaOS keyboard/mouse/joystick events. |
| `input_exclusive` | Runtime polling/interrupt capture and emergency stop. |
| `platform_amigaos` | C runtime integration plus AmigaOS libraries, memory classes, timing, startup, shutdown, and resource ownership. |
| `takeover` | Transactional state machine for exclusive entry and restoration. |

The compiler frontend, typed IR, 68020 emitter, cartridge parser, MOD state machine, and reference renderer SHOULD also compile natively on the host for fast tests. A host-only typed-IR interpreter or simulator SHOULD provide a differential oracle for generated 68020 execution. Platform dependencies MUST sit behind narrow interfaces.

### 16.2 C99 and assembly policy

- All shipping target source MUST be C99 except documented `.s`/`.S` files.
- Compiler-specific attributes and register conventions MUST be isolated in platform headers.
- Assembly is justified for interrupt entry/exit, exact custom-chip access sequences, chunky-to-planar conversion, and proven rendering hot spots.
- Every assembly optimization MUST have a readable C reference or a precise behavioral test oracle.
- Assembly functions MUST document clobbered registers, stack alignment, memory alignment, address-space requirements, and C ABI.
- No assembly optimization may be merged without real-hardware measurements.
- User cartridges contain no hand-authored native assembly or C and MUST NOT embed arbitrary machine code; only the trusted MIGA Lua compiler may populate the native code arena.

### 16.3 Toolchain

The preferred toolchain is the maintained `m68k-amigaos-gcc` family with AmigaOS NDK-compatible headers and libraries. It builds MAGI-80 itself; it is not invoked by the on-Amiga MIGA Lua compiler. The build MUST pin an exact toolchain commit or reproducible container image even though the product does not require a particular GCC version.

Initial MAGI-80 system compiler/linker policy:

- C language mode: `-std=c99`;
- CPU baseline: `-m68020` or the verified equivalent;
- no hardware FPU assumption; target code MUST not introduce FPU instructions;
- C runtime: libnix Kickstart 2+ startup selected with `-mcrt=nix20`, placed last on the link command;
- optimize for size for cold/editor code and benchmarked speed for hot paths;
- emit an AmigaOS Hunk executable compatible with the target Kickstart versions;
- avoid ixemul and other non-stock runtime dependencies;
- retain a host-side symbol/map file while stripping release debugging data from the floppy binary;
- treat warnings as errors in project code;
- generate a size report by object and section for every release build.

The initial system ABI and toolchain revisions are locked in `toolchain/versions.lock`. The runtime matrix selected libnix over clib2 on size after both passed the required allocation/filesystem tests under `vamos` and FS-UAE/Kickstart 3.0; newlib failed required semantics and introduced an external math-library startup dependency. Kickstart 3.1 and real-A1200 results remain mandatory revalidation gates, and any resulting ABI change MUST be recorded explicitly.

### 16.4 Build outputs

One command SHOULD produce:

- host unit-test binaries;
- debug Amiga Hunk executable and map;
- stripped release executable;
- hard-disk installation directory;
- legal bootable ADF when required licensed inputs are supplied;
- image manifest with hashes and byte/block usage;
- example `.m80` cartridge;
- test report for compiler, 68020 code generation/native runtime, graphics, MOD parsing, and packaging.

## 17. Error handling and data safety

### 17.1 Error classes

MAGI-80 MUST distinguish:

- startup/platform incompatibility;
- insufficient or fragmented memory;
- DOS/device/media error;
- invalid cartridge/container;
- compile error;
- type/IR error or native code-generation/validation error;
- controlled runtime fault;
- resource acquisition/takeover failure;
- internal invariant failure.

Expected user errors return to the relevant editor. A takeover failure unwinds to hosted mode. An internal error before takeover should exit cleanly when safe. A fatal hardware-state error may require a reboot; the documentation must be honest about this residual risk.

### 17.2 Project safety

- The in-memory project remains authoritative until a save completes.
- Compile and run MUST NOT implicitly mark an unsaved project as saved.
- The UI MUST display unsaved state before quit or disk exchange.
- Save failures MUST retain the old valid file and in-memory edits whenever possible.
- Checksums detect corruption but do not replace safe write ordering.
- Recovery files MAY be written to hard disk, but automatic floppy writes are off by default.

### 17.3 Untrusted input

Cartridges and modules are untrusted input. Host builds MUST run their parsers under sanitizers and fuzzing. Target builds MUST use checked arithmetic for offsets, sizes, counts, multiplication, decompression bounds, and sample-loop calculations.

## 18. Verification strategy

### 18.1 Test layers

1. **Host unit tests:** lexer, Lua-like grammar, type inference/rules, typed IR, record/array/dictionary layouts, 68020 instruction encoding and relocation, native ABI guards, fixed-point math, cartridge parser, compression, MOD effect state, and graphics primitives.
2. **Property and fuzz tests:** cartridge/MOD parsing, decompression, source lexer/parser, typed IR, relocation inputs, dictionary operations, and checked arithmetic.
3. **Golden and differential tests:** rendered frame hashes, C2P plane bytes, palette mapping, emitted 68020 words/disassembly, generated-code behavior versus a host typed-IR oracle, source error locations, runtime traces, and MOD tick traces.
4. **Emulator integration:** PAL A1200, 68020, AGA, exactly 2 MiB Chip RAM, no Fast RAM; floppy boot, HD launch, disk swaps, and error paths.
5. **Real-hardware tests:** at least two stock A1200 units if available, original or representative floppy drive/media, CRT and modern display adapter where relevant.
6. **Soak tests:** editor idle, music preview, runtime, repeated run/stop, repeated save/load, and low-memory behavior.

Emulators are essential for automation but cannot sign off custom-chip timing, Chip-RAM contention, CIA behavior, floppy reliability, or restoration.

### 18.2 Required compatibility matrix

| Configuration | Floppy boot | HD CLI | Workbench launch | Edit | Run/restore |
| --- | --- | --- | --- | --- | --- |
| PAL A1200, KS/WB 3.0, 2 MiB only | Required | Required | Required | Required | Required |
| PAL A1200, KS/WB 3.1, 2 MiB only | Required | Required | Required | Required | Required |
| PAL A1200, 3.0/3.1 with Fast RAM | Required | Required | Required | Required | Required |
| NTSC A1200 | Safe rejection | Safe rejection | Safe rejection | Not certified | Not certified |
| AGA emulator, exact stock profile | Required in CI | Required in CI | Recommended | Required | Required, then confirm on hardware |

### 18.3 Release acceptance criteria

MAGI-80 1.0 is complete only when all of the following are true:

- A legal distribution artifact fits and boots from one real DD floppy.
- The hard-disk edition launches from CLI and Workbench.
- A user can create a project, edit source, draw sprites, import a supported MOD, save, reload, compile, run, stop, and continue editing on a stock A1200.
- The included example cartridge demonstrates input, both playfields, transparency, primitives, sprites, text, music, and a controlled runtime error screen.
- Full-frame conversion sustains the certified 25 Hz content profile without tearing or starving music.
- Compiler, generated native code, and runtime behavior are deterministic across host reference tests, emulator, and hardware.
- Malformed corpus and fuzz regressions do not crash or corrupt memory.
- The 1,000-cycle takeover/restoration test passes.
- A 30-minute representative runtime and a two-hour hosted editing/music-preview soak pass.
- Peak memory and ADF block usage remain within frozen release budgets.
- User documentation describes controls, limits, file compatibility, backup practices, and unsupported configurations.

## 19. Development roadmap

The estimates below are engineering effort for one experienced developer, not calendar promises. They include implementation and ordinary testing but not large hardware-procurement or licensing delays. The critical path is hardware feasibility first, then safe on-target native compilation and the content workflow, then hardening.

### Phase 0 — Feasibility and irreversible decisions

**Estimate:** 4–6 engineer-weeks

Deliverables:

- reproducible GCC cross-toolchain and minimal C99 Hunk executable;
- bootable test ADF and HD launch test on Kickstart 3.0/3.1;
- exact empty-boot and Workbench-launch memory measurements;
- AGA 256 × 256 4+4 dual-playfield display with the 31-color mapping;
- reference C and candidate 68020 assembly C2P benchmarks;
- minimal strongly typed expression/function compiler that emits and safely invokes native 68020 code on the A1200;
- generated-code instruction-cache synchronization and stop-at-loop-backedge proof;
- prototype hosted-to-exclusive takeover and restoration loop;
- joystick and keyboard emergency-stop prototype;
- Paula four-channel playback of a minimal MOD with candidate timing backends;
- measured OFS/FFS image payload capacity;
- licensing decision record for the boot disk.

Exit gate:

- The display is stable on real PAL A1200 hardware.
- At least 25 full logical frames per second are feasible with audio enabled and useful CPU headroom, or the graphics model is revised explicitly.
- The native compiler can emit, relocate, validate, cache-synchronize, run, budget-stop, and discard a small guarded 68020 program without destabilizing AmigaOS.
- Run/restore succeeds 100 consecutive times without a leak or broken OS state.
- A credible path exists to stay below 1,388 KiB peak MAGI-80 memory and 800 KiB disk payload.
- The project has a legal route to a bootable release image.

If this gate fails, do not build editors. Reduce the video conversion cost, memory model, cartridge cap, or boot scope first. If the native compiler itself fails its time, size, cache, or safety gate, simplify its language/backend or explicitly revisit the bytecode fallback before proceeding; do not maintain two production execution engines.

### Phase 1 — Platform foundation and hosted shell

**Estimate:** 4–6 engineer-weeks

Deliverables:

- platform library/resource wrappers with strict ownership tracking;
- Chip/Fast memory arenas and low-memory diagnostics;
- hosted 256 × 256 screen, font, widgets, and command routing;
- keyboard, mouse, and joystick hosted input;
- DOS volume browser and bounded file reader;
- safe shutdown and startup error paths;
- hard-disk install layout and Workbench icon prototype;
- continuous emulator smoke test.

Exit gate:

- Shell boots from ADF and HD, remains responsive under AmigaOS multitasking, browses OFS/FFS, and exits cleanly on both target OS versions.

### Phase 2 — Graphics, input, and exclusive runtime core

**Estimate:** 5–7 engineer-weeks

Deliverables:

- frozen virtual graphics semantics and portable reference renderer;
- optimized C2P, double buffering, palette mapping, and Copper/display backend;
- primitive and software-sprite renderer;
- exclusive input backend and protected emergency stop;
- transactional takeover/restoration implementation;
- frame scheduler, deterministic 25/50 Hz modes, and profiler;
- soak and 1,000-cycle restoration harness.

Exit gate:

- A native C test scene runs at the certified rate with both layers, sprites, input, and clean restoration on real stock hardware.

### Phase 3 — MIGA Lua compiler and native 68020 backend

**Estimate:** 9–14 engineer-weeks

Deliverables:

- versioned MIGA Lua reference, Lua 5.1 compatibility matrix, and grammar;
- lexer, parser, strong type inference/checking, fixed-point semantics, AST, and typed IR;
- fixed-layout records, arrays, optionals, and deterministic fixed-capacity dictionaries;
- 68020 data/stack layout, instruction selector, register/stack allocation, binary emitter, relocation, and generated-code validator;
- versioned native ABI, immutable runtime jump table, typed graphics/input/audio bindings, and stop/error trampolines;
- bounds/division/shift/stack guards plus execution-budget and stop checks at every backward control-flow edge;
- generated-code arena ownership and Exec instruction-cache synchronization;
- deterministic random, lifecycle, source line maps, and runtime diagnostic capture;
- host typed-IR oracle, emitter/disassembly golden tests, differential execution tests, and relocation/IR fuzz targets;
- native-host compiler test build and on-target compile-time/code-size reports.

Exit gate:

- A nontrivial MIGA Lua game compiles to native 68020 code on the stock A1200, runs deterministically, detects an infinite loop and representative guarded faults, and returns source-level errors after restoring AmigaOS.

### Phase 4 — Cartridge, code editor, and sprite editor

**Estimate:** 7–10 engineer-weeks

Deliverables:

- versioned `.m80` container and bounded compression;
- safe-load and safe-save workflow;
- source editor with navigation, diagnostics, search, and bounded undo;
- sprite/palette editor with required tools and preview;
- budget meters and project metadata;
- integrated compile/run/stop loop preserving editor state;
- example cartridge graphics and source.

Exit gate:

- A user can create, save, reload, edit, compile, and run a small sprite-based game entirely on a stock A1200.

### Phase 5 — ProTracker import and integrated audio

**Estimate:** 4–7 engineer-weeks

Deliverables:

- defensive MOD importer and compatibility report;
- required ProTracker effect set and trace corpus;
- hosted preview backend;
- exclusive Paula/CIA or accepted timing backend;
- cartridge music controls and size accounting;
- audio restoration and soak tests.

Exit gate:

- Supported corpus modules preview and run correctly, malformed modules fail safely, timing traces match the declared reference, and no stuck DMA/channel state survives stop.

Audio may begin as a Phase 0 spike and proceed in parallel with Phases 3–4 once the ownership model is fixed, but the final integration gate remains here.

### Phase 6 — Floppy productization and release hardening

**Estimate:** 6–8 engineer-weeks

Deliverables:

- final boot image generation and legal packaging route;
- program/data size optimization and locked budgets;
- help screens, templates, example cartridge, and user manual;
- complete low-memory, disk-full, write-protected, disk-swap, and corrupt-input behavior;
- compatibility matrix and real-hardware sign-off;
- performance tuning driven by profiler evidence;
- release candidate soak, recovery, and regression runs;
- reproducible build manifest and source release packaging.

Exit gate:

- Every release acceptance criterion in section 18.3 passes.

### 19.1 Overall estimates

| Milestone | Cumulative estimate | Outcome |
| --- | ---: | --- |
| Feasibility gate | 4–6 weeks | Core hardware, memory, disk, legal model, and minimal native compiler path proven. |
| Runtime prototype | 13–19 weeks | Native C test scene with safe takeover, graphics, input, and audio foundations. |
| Creator alpha | 29–43 weeks | On-machine MIGA Lua-to-68020 compiler, code editor, sprite editor, cartridges, and integrated run loop. |
| Version 1.0 | 39–58 weeks | Floppy/HD packaging, MOD compatibility, native-code hardening, docs, and real-hardware certification. |

A solo part-time project should translate these into milestones rather than fixed dates. Schedule contingency belongs mainly to takeover/restoration, C2P performance, CIA/keyboard behavior, floppy packaging, and editor usability.

## 20. Prioritization and scope cuts

If measured constraints force reductions, cut in this order:

1. syntax coloring and advanced editor commands;
2. optional MIGA Lua conveniences such as non-capturing function values, method-call sugar, and generic dictionary iteration;
3. 50 Hz simulation mode;
4. optional ProTracker effects and hosted live preview;
5. map data and `map_draw`;
6. sprite transform conveniences beyond flip;
7. multiple example cartridges and expanded on-disk help;
8. compression sophistication.

Do not cut:

- restoration safety;
- stock 2 MiB support;
- on-Amiga MIGA Lua compilation to native 68020 code;
- strong types, fixed data layouts, generated-code guards, stop safe points, and the closed native ABI;
- the code and sprite editors;
- the two-layer 256 × 256 graphics contract;
- basic supported MOD import/playback;
- OFS/FFS access through AmigaDOS;
- both floppy and HD launch routes;
- bounded input parsing and guarded native execution.

## 21. Risk register

| ID | Risk | Probability | Impact | Mitigation and trigger |
| --- | --- | --- | --- | --- |
| R-01 | “Replace AmigaOS” is interpreted as bare metal, conflicting with library-backed filesystems and live editing. | High | Critical | Adopt the two-personality hosted/exclusive definition. Any demand for literal bare metal triggers a separate architecture and roadmap. |
| R-02 | Full 8-plane C2P plus game logic cannot sustain 50 Hz on stock Chip RAM. | High | High | Certify 25 Hz first, benchmark in Phase 0, optimize in assembly, exploit dirty/no-change frames, and keep frame rate explicit. |
| R-03 | 2 MiB Chip RAM is insufficient or too fragmented after Workbench launch. | High | High | Preflight total/largest blocks, reserve critical buffers at launch, reuse arenas, offer minimal floppy boot, and fail before takeover with a size report. |
| R-04 | Long `Forbid()` sessions or incorrect interrupt/resource handling destabilize AmigaOS. | Medium | Critical | No waiting calls in runtime; avoid long `Disable()`; isolate takeover; record ownership; stress 1,000 transitions; test multiple OS versions and real machines. |
| R-05 | Keyboard/CIA and ProTracker timer ownership conflict. | Medium | Critical | Prototype both in Phase 0, use OS resources to reserve hardware, select distinct or shared interrupt-safe scheduling, and make emergency stop independent of generated code and audio. |
| R-06 | Generated native code crashes after takeover and cannot restore hardware. | Medium | Critical | Strong types, no pointers or imported machine code, guarded operations, budget/stop checks at backward edges, a closed jump-table ABI, validated emission/relocation, differential tests, small interrupt handlers, and a documented residual reboot risk because the 68EC020 provides no process sandbox. |
| R-07 | The bootable disk cannot legally include required AmigaOS files. | High until resolved | Critical | Decide installer/licensing/compatible-component route in Phase 0. No public boot image until resolved. |
| R-08 | The executable and useful example exceed floppy capacity. | Medium | High | Continuous block-budget CI, size maps, embedded compact assets, `-Os` for cold code, optional compressor, and reduced examples/help before features essential to creation. |
| R-09 | ProTracker compatibility expands without bound because historical players disagree on effects. | High | Medium | Declare signatures and effects, use trace-based conformance, warn for unsupported behavior, and version the compatibility profile. |
| R-10 | Malformed MOD or cartridge data causes overflow or corruption. | Medium | Critical | Checked arithmetic, bounds-first parsing, host fuzzing/sanitizers, target corpus, checksums, no native-code sections in cartridges, and no code generation until all source/assets are validated. |
| R-11 | Lua familiarity drives the language toward dynamic general-purpose Lua and erodes predictable native layouts. | High | High | Freeze the Lua compatibility matrix, retain strong types/inference, fixed records/arrays/dictionaries, no universal table or GC, and reject features without a representative-game need. |
| R-12 | A 256-pixel-wide code editor is frustrating. | Medium | High | Test 5–6-pixel fonts early, guarantee horizontal scrolling and shortcuts, keep UI chrome minimal, and consider an optional hosted high-resolution editor only after the core workflow works. |
| R-13 | Emulator success hides real Chip-RAM contention or timing faults. | High | High | Real-hardware gate in every hardware-facing phase; use emulators for regression, never final timing sign-off. |
| R-14 | Physical floppy writes are slow or unreliable and safe replacement needs extra free space. | High | Medium | Keep projects in memory, show writes, use temporary-file replacement, encourage separate project disks/HD, verify after write, and never autosave to floppy by default. |
| R-15 | AGA details such as PF2 palette offsets or fetch alignment vary from assumptions. | Medium | High | Freeze behavior from a minimal register test using Commodore documentation, capture working register values, and add plane/palette golden screens. |
| R-16 | C runtime or GCC output pulls in large or non-stock dependencies. | Medium | High | Inspect link maps from day one, avoid floating point and heavyweight stdio, use a minimal compatible runtime, and pin the toolchain/ABI. |
| R-17 | Hosted module preview cannot acquire all Paula channels without disrupting other applications. | Medium | Low | Make preview cooperative and optional, report contention, and reserve exclusive guarantees for Run mode. |
| R-18 | Disk swapping prompts for the boot volume after MAGI-80 starts. | Medium | Medium | Embed fonts/help needed after launch, avoid overlays initially, pre-open/close required libraries and files, and test single-drive boot-to-project swaps. |
| R-19 | The on-target native compiler is too large or too slow for an edit–run loop. | Medium | High | Prove a minimal emitter in Phase 0, use compact typed IR and bounded arenas, prefer simple passes, measure per-phase time/peak memory/code size, and cache the compiled image only within the trusted current session. |
| R-20 | A code-emitter or relocation bug generates a legal-looking but unsafe 68020 instruction stream. | Medium | Critical | Template-driven emission, independent host disassembly/golden tests, typed relocations, code-range and call-target validation, differential execution against a host IR oracle, and aggressive malformed-IR fuzzing. |
| R-21 | The 68020 executes stale instructions after code generation. | Medium | Critical | Never execute before relocation and validation complete; call the appropriate Exec cache-clear function over the generated range before takeover; include repeated compile/run code-replacement tests on real hardware. |

## 22. Decision log required before implementation freeze

The following decisions must be recorded with measurements or prototypes:

1. Exact certified Kickstart/Workbench versions.
2. OFS or FFS format for the boot image and actual payload ceiling.
3. Legal source of boot components.
4. Exact hosted display API and screen depth.
5. Exact AGA bitplane, priority, palette, fetch, modulo, and Copper settings.
6. Combined-byte versus separate-layer chunky layout.
7. C2P implementation and certified 25/50 Hz performance envelope.
8. Runtime keyboard acquisition and emergency-stop mechanism.
9. CIA-timed versus fractional-VBlank ProTracker scheduler.
10. Selected GCC fork, commit, NDK, C runtime, ABI, and release flags.
11. Frozen MIGA Lua grammar, Lua 5.1 compatibility matrix, type inference rules, numeric semantics, and restricted feature set.
12. Record/array/dictionary layouts, supported key types, hash/probing algorithm, capacities, load factor, and deterministic iteration order.
13. Native 68020 ABI, runtime-context convention, jump table, relocation model, code arena, stack rules, guards, loop budgets, stop safe points, and cache-clear sequence.
14. Final source, generated-code, packed-cartridge, resident-cartridge, undo, and module limits.
15. Cartridge checksum and compression algorithms.
16. Safe-save behavior when a floppy cannot hold old and temporary copies.

## 23. Recommended first vertical slice

After Phase 0, the first end-to-end slice SHOULD be deliberately small:

1. Boot from ADF into the hosted shell.
2. Open a hard-coded 20-line strongly typed MIGA Lua example in a minimal editor.
3. Compile it on the A1200 directly to guarded native 68020 code, validate/relocate it, and synchronize the instruction cache.
4. Enter exclusive mode.
5. Read joystick input.
6. Move one software sprite on the foreground over a patterned background.
7. Play one validated looping MOD.
8. Stop with the protected key chord.
9. Restore the editor at the same cursor position.
10. Save and reload the single-file cartridge through AmigaDOS.

This slice exercises every architectural boundary before richer editor tools or language features make failures harder to isolate.

## 24. References and technical basis

These sources inform the assumptions above; they are not runtime dependencies:

- [AmigaOS Documentation Wiki — Exec Tasks](https://wiki.amigaos.net/wiki/Exec_Tasks), especially the behavior of `Forbid()`, `Permit()`, `Disable()`, `Enable()`, and the warning against long disabled sections.
- [AmigaOS Documentation Wiki — Exec processor and cache control](https://wiki.amigaos.net/wiki/Exec_Tasks#Processor_and_Cache_Control), for `CacheClearE()`, `CacheClearU()`, the 68020 instruction cache, and generated/self-modifying code synchronization.
- [AmigaOS Documentation Wiki — Classic Graphics Primitives](https://wiki.amigaos.net/wiki/Classic_Graphics_Primitives), covering `View`, dual playfields, double buffering, direct blitter coordination, and display construction.
- [AmigaOS Documentation Wiki — Graphics Primitives](https://wiki.amigaos.net/wiki/Graphics_Primitives), including `LoadView()`, `OwnBlitter()`, `WaitBlit()`, and `DisownBlitter()`.
- [AmigaOS Documentation Wiki — Basic Input and Output Programming](https://wiki.amigaos.net/wiki/Basic_Input_and_Output_Programming), for AmigaDOS locks and file-handle operations.
- [Commodore Amiga Hardware Reference Manual mirror — Forming a Dual-Playfield Display](https://amigadev.elowar.com/read/ADCD_2.1/Hardware_Manual_guide/node0078.html), for odd/even bitplane grouping, transparency, palette grouping, and playfield priority.
- [Commodore AA Chip Set Functional Specification mirror](https://shanson.com/spencer/Amiga-AA-Chipset.pdf), for AGA's eight-bitplane, palette-bank, fetch-mode, and `PF2OF` extensions.
- [NXP/Freescale MC68020/MC68EC020 User's Manual](https://www.nxp.com/docs/en/data-sheet/MC68020UM.pdf), for the certified code generator's instruction set, cache, exception, and timing model.
- [Lua 5.1 Reference Manual](https://www.lua.org/manual/5.1/manual.html) and [official Lua 5.1 source](https://www.lua.org/source/5.1/), used to define and test the documented boundary between familiar Lua syntax and MIGA Lua's static semantics.
- [AmigaPorts m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc), the preferred starting point for the cross-compilation toolchain.
- [8bitbubsy pt2-clone](https://github.com/8bitbubsy/pt2-clone), a useful declared behavioral comparison point for ProTracker replay tests; reuse of code would require a separate license review.

Before direct custom-chip code is frozen, the project SHOULD archive the exact Commodore/NDK documentation revision used and cite register pages in a dedicated hardware design note.
