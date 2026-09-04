# MIGA Lua Optimization Strategy

**Status:** acyclic CFG, typed `phi` joins, `-O1`, spilling, and frame layout implemented

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

The typed stack IR now has up to 32 explicit basic blocks, bounded successor
slots, typed local load/store operations, comparisons, and branch terminators.
The current acyclic `if`/`else` CFG renames each local to its current value and
inserts typed `phi` values at two-predecessor joins. It folds constants with
defined 32-bit wrapping, applies algebraic identities, marks dead values and
overwritten assignments, computes conservative live intervals, and allocates
`D0-D7` with a bounded linear scan. Used `D3-D7` registers are preserved once
with `MOVEM`; expression values are not pushed to `A7`. Constant multiplication
by two or three is strength-reduced when the available registers make that
profitable.

The allocator first tries all eight data registers, so ordinary leaf functions
do not lose `D7` merely to reserve a scratch register. If that plan needs a
spill, a second bounded pass allocates values in `D0-D6`, reserves `D7` as the
spill scratch, and reuses four-byte spill slots after their last use. The
backend emits an ABI 0.1 `A6` frame with `LINK`/`UNLK`, addresses slots at
negative `A6` offsets, consumes spilled operands directly as 68020 memory
operands where possible, and preserves `D7` with the other used saved
registers. Frame size is checked before any assembly is emitted.

`-O0` gives every source local a physical negative `A6` slot after the
parameter slots. `-O1` needs no local slot when straight-line renaming and
liveness keep the value in registers. Across branches, it currently assigns a
distinct bounded spill slot to every live `phi` and copies the selected value
on the incoming edge. This is deliberately conservative and cycle-safe, but
the conditional corpus shows that `phi`-slot coalescing and CFG-aware register
liveness are the next concrete stack/performance optimization. Cyclic CFG
construction and loop-carried `phi` values for `while`, copy propagation
across joins, address-register allocation, and the shared low-level m68k model
remain pending. Global value numbering,
expensive interprocedural optimization, speculative dynamic typing, and
exhaustive instruction scheduling remain excluded until measurements justify
their compiler cost.

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

The first optimization milestone is met. Across five reviewed source corpora
and six edge inputs each, both optimization levels agree with the typed IR
under Musashi. The current regression measurements are shown as code bytes /
executed instructions / maximum callee stack bytes:

| Corpus | `-O0` | `-O1` |
| --- | ---: | ---: |
| arithmetic and parameter reuse | 116 / 41 / 28 | 28 / 12 / 4 |
| folding, negation, and identities | 132 / 46 / 28 | 28 / 10 / 4 |
| register pressure and repeated products | 120 / 43 / 32 | 40 / 13 / 8 |
| typed locals, reassignment, and dead store | 160 / 53 / 32 | 28 / 12 / 4 |
| six comparisons and nested `if`/`else` | 572 / 123 / 32 | 388 / 64 / 44 |

The register-pressure corpus forces simultaneous `D3/D4` allocation and
verifies their ABI preservation. A separate deliberately pressure-heavy value
IR fixture forces three reusable spill slots; its 96-byte image executes 33
instructions with 36 callee stack bytes and agrees with the source-level oracle
for six edge inputs. This brings the current Musashi compiler total to 66
executions.

The `-Os` 68020/libnix compiler currently has a 40,700-byte linked
text/data/BSS footprint. Its host and Amiga builds emit byte-identical ordinary
local-heavy, conditional, and spilling `-O1` assembly. The conditional corpus
reduces code by 32% and executed instructions by 48% at O1, while its current
44-byte callee stack is higher than O0's 32 bytes because of conservative
`phi` slots. These counts are regression signals, not A1200 cycle claims.
