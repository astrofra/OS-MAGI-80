# MIGA Lua Optimization Strategy

**Status:** initial straight-line value IR and `-O1` implemented

MIGA Lua is a statically typed, ahead-of-time compiled dialect designed for
native 68EC020/68020 code. Familiar Lua syntax is a usability goal. Dynamic Lua
values, universal tables, metatables, garbage collection, runtime compilation,
and full Lua compatibility are not goals. Unsupported constructs are rejected
instead of silently selecting a dynamic fallback.

## Two performance budgets

The project must optimize both the generated program and the compiler running
on a stock A1200. The typed stack IR and `-O0` stack-heavy assembly renderer are
correctness oracles. They deliberately make evaluation order obvious and are
not a production-quality code-generation strategy.

The shipping compiler remains portable C99 built for the 68020. It processes
one function at a time with bounded reusable arenas, compact integer indexes,
and deterministic passes. It must not require an assembler, linker, garbage
collector, unbounded recursion, or host-sized optimization data structures.
The frontend, value-IR optimizer, and assembly renderer are cross-built with
libnix and executed under `vamos` by `gmake compiler-amiga-test`. That test
requires byte-identical textual assembly from the host and 68020 compiler
builds. Direct native emission on the Amiga remains a separate later gate. The
same target retains the bootstrap executable size in
`build/reports/compiler-amiga-size.txt` so compiler growth remains visible.

## Planned bounded pipeline

```text
typed semantic IR
    -> basic blocks and value/three-address IR
    -> constant folding and propagation
    -> algebraic simplification and copy propagation
    -> dead and unreachable code removal
    -> 68020 instruction selection and strength reduction
    -> liveness and linear-scan register allocation
    -> stack/frame layout
    -> small final peephole pass
    -> shared low-level m68k instruction model
```

The straight-line `i32` bootstrap now lowers into the value IR. It folds
constants with defined 32-bit wrapping, applies algebraic identities, marks
dead values, computes live intervals, and allocates `D0-D7` with a bounded
linear scan. Used `D3-D7` registers are preserved once with `MOVEM`; expression
values are not pushed to `A7`. Constant multiplication by two or three is
strength-reduced when the available registers make that profitable.

Register spilling, basic blocks, copy propagation across control-flow joins,
address-register allocation, frame layout for locals, and the shared low-level
m68k model remain pending. The current `-O1` reports a bounded register-pressure
diagnostic instead of generating incorrect code when its eight data registers
are insufficient. Global value numbering, expensive interprocedural
optimization, speculative dynamic typing, and exhaustive instruction
scheduling remain excluded until measurements justify their compiler cost.

The first 68020-specific choices include keeping scalars in data registers,
keeping future addresses in address registers, folding constant displacements
into effective addresses, preferring compact immediate forms, strength-reducing
constant multiplication when profitable, arranging fall-through branches, and
removing redundant extensions, moves, loads, and stores. Speed and code size
are separate costs because the A1200 instruction cache and memory domain can
make a shorter sequence preferable to one with a nominally lower instruction
count.

## Optimization levels

- `-O0` retains the implemented direct, debuggable lowering and is the semantic
  baseline.
- `-O1` is the implemented default: bounded simplification, dead-value
  removal, instruction selection, linear-scan allocation, and immediate-form
  selection.
- A later `-O2` may spend more compile time on the host and capable profiles,
  but it must use the same semantics and ABI. It must never be required to run
  a cartridge on a stock A1200.

Every optimization must preserve defined 32-bit wrapping, evaluation order,
guards, stop checks, and observable runtime calls. `-O0` and `-O1` are compared
against the same typed-IR oracle and executed under Musashi with deterministic
inputs. The same rule will apply to `-O2` when it exists.

## Measurement and acceptance

Musashi validates instruction semantics and ABI behavior; it is not a
cycle-accurate A1200 performance oracle. Compiler regressions will record code
bytes, executed instruction count, maximum generated stack use, calls, and
memory-operation widths. Stable curated kernels may receive reviewed limits.
FS-UAE remains an integration check, while timing decisions require repeated
measurements on a stock physical A1200 under a declared DMA and memory profile.

The first optimization milestone is met. Across three reviewed expression
corpora and six edge inputs each, both optimization levels agree with the typed
IR under Musashi. The current regression measurements are:

| Corpus | `-O0` bytes / instructions | `-O1` bytes / instructions |
| --- | ---: | ---: |
| arithmetic and parameter reuse | 116 / 41 | 28 / 12 |
| folding, negation, and identities | 132 / 46 | 28 / 10 |
| register pressure and repeated products | 120 / 43 | 40 / 13 |

The register-pressure corpus forces simultaneous `D3/D4` allocation and
verifies their ABI preservation. The `-Os` 68020/libnix compiler currently has
a 27,264-byte linked text/data/BSS footprint; its host and Amiga builds emit
byte-identical `-O1` assembly. These counts are regression signals, not A1200
cycle claims.
