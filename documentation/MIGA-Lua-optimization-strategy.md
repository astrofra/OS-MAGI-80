# MIGA Lua Optimization Strategy

**Status:** architecture accepted; optimization implementation pending

MIGA Lua is a statically typed, ahead-of-time compiled dialect designed for
native 68EC020/68020 code. Familiar Lua syntax is a usability goal. Dynamic Lua
values, universal tables, metatables, garbage collection, runtime compilation,
and full Lua compatibility are not goals. Unsupported constructs are rejected
instead of silently selecting a dynamic fallback.

## Two performance budgets

The project must optimize both the generated program and the compiler running
on a stock A1200. The current typed stack IR and stack-heavy assembly renderer
are correctness oracles. They deliberately make evaluation order obvious and
are not a production-quality code-generation strategy.

The shipping compiler remains portable C99 built for the 68020. It processes
one function at a time with bounded reusable arenas, compact integer indexes,
and deterministic passes. It must not require an assembler, linker, garbage
collector, unbounded recursion, or host-sized optimization data structures.
The current frontend and typed-IR evaluator are cross-built with libnix and
executed under `vamos` by `gmake compiler-amiga-test`; direct native emission
on the Amiga remains a separate later gate. The same target retains the
bootstrap executable size in `build/reports/compiler-amiga-size.txt` so growth
is visible before optimizer work begins.

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

The value IR is the next compiler representation. It removes the artificial
operand-stack traffic without changing the existing typed IR oracle. Initial
passes are local or function-wide and near-linear in instruction count. Global
value numbering, expensive interprocedural optimization, speculative dynamic
typing, and exhaustive instruction scheduling are excluded until measurements
justify their compiler cost.

The first 68020-specific choices include keeping scalars in data registers,
keeping future addresses in address registers, folding constant displacements
into effective addresses, preferring compact immediate forms, strength-reducing
constant multiplication when profitable, arranging fall-through branches, and
removing redundant extensions, moves, loads, and stores. Speed and code size
are separate costs because the A1200 instruction cache and memory domain can
make a shorter sequence preferable to one with a nominally lower instruction
count.

## Optimization levels

- `-O0` retains a direct, debuggable lowering and is the semantic baseline.
- `-O1` is the default on-Amiga mode: bounded simplification, dead-code
  removal, instruction selection, linear-scan allocation, and peepholes.
- `-O2` may spend more compile time on the host and on capable Amiga profiles,
  but it must use the same semantics and ABI. It must never be required to run
  a cartridge on a stock A1200.

Every optimization must preserve defined 32-bit wrapping, evaluation order,
guards, stop checks, and observable runtime calls. `-O0`, `-O1`, and `-O2`
outputs will be compared against the same typed-IR oracle and executed under
Musashi with deterministic inputs.

## Measurement and acceptance

Musashi validates instruction semantics and ABI behavior; it is not a
cycle-accurate A1200 performance oracle. Compiler regressions will record code
bytes, executed instruction count, maximum generated stack use, calls, and
memory-operation widths. Stable curated kernels may receive reviewed limits.
FS-UAE remains an integration check, while timing decisions require repeated
measurements on a stock physical A1200 under a declared DMA and memory profile.

The first optimization milestone is the existing arithmetic fixture. Its
optimized form must keep expression temporaries out of `A7`, preserve ABI 0.1,
and agree bit-for-bit with the typed-IR oracle for edge and deterministic random
inputs. This milestone precedes branches, loops, and general function calls.
