# MIGA Lua Optimization Strategy

**Status:** normalized loops, cyclic CFG liveness, branch/loop `phi`, parallel edge copies, `-O1`, spilling, and frame layout implemented

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

## IR memory budget

The lexer already streams over the source and retains only its current token;
it does not materialize a token list. The bootstrap AST and IR arenas instead
favor explicit, easily checked C99 records while the semantics are still
moving. With the current 32-bit enum/index ABI, an AST node occupies 32 bytes,
a typed stack-IR instruction 16 bytes, a stack-IR block 20 bytes, a value-IR
instruction 48 bytes, and a value-IR block 40 bytes. At all configured maxima,
an `-O1` compilation with a 64 KiB source uses approximately 99 KiB for the
source buffer, main arenas, CFG bitsets, and optimizer workspace, excluding
libc buffers and the compiler's own stack.

This is a measured design budget, not the final representation. Once the
remaining control-flow semantics are stable, the production layout SHOULD use
16-bit value, instruction, block, and symbol indexes with `0xffff` as the invalid value; a
16-bit opcode/type/flag header; aligned fixed records of roughly 8 or 12 bytes;
and a separate compressed source-location table. Liveness and allocation
scratch tables SHOULD share reusable per-function arenas. Blindly packed C
structures and an opaque variable-length token stream are not preferred: the
former risks inefficient unaligned 68020 accesses, while the latter makes
random optimizer access and CFG rewriting more expensive and complex.

## Planned bounded pipeline

```text
typed semantic IR
    -> basic blocks and value/three-address IR
    -> constant folding and propagation
    -> algebraic simplification and copy propagation
    -> dead and unreachable code removal
    -> 68020 instruction selection and strength reduction
    -> CFG liveness and bounded register allocation
    -> stack/frame layout
    -> small final peephole pass
    -> shared low-level m68k instruction model
```

The typed stack IR now has up to 32 explicit basic blocks, bounded successor
slots, typed local load/store operations, comparisons, branches, and backward
edges. `if`/`else` and `while` CFGs rename each local to its current value and
insert typed `phi` values at two-predecessor joins. Lowering normalizes every
loop as `preheader -> header -> body -> dedicated latch -> header`, with the
false header edge targeting one dedicated exit. A bounded dominator pass
recognizes the natural loop and verifies that it has exactly that single
preheader, latch, and exit. Loop-header `phi` values are deliberately created
before their backward operands exist, then completed after the latch is
lowered; trivial self-joins are removed. The dead-value walk is a bounded
worklist rather than a reverse linear scan, so cyclic dependencies and forward
value indexes are valid.

The optimizer folds constants with defined 32-bit wrapping, applies algebraic
identities, and marks dead values and overwritten assignments. A bounded
fixed-point data-flow pass computes `live-in` and `live-out` bitsets for every
block, including cyclic CFGs; `phi` operands are uses on their incoming CFG
edges rather than ordinary uses in the join block. The allocator reconstructs
live register and spill ownership at each block entry, so mutually exclusive
branches may reuse locations. Used `D3-D7` registers are preserved once with
`MOVEM`; expression values are not pushed to `A7`. Constant multiplication by
two or three is strength-reduced when the available registers make that
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
parameter slots. `-O1` needs no local slot when value renaming and liveness keep
the value in registers. Across branches and loops, it colors `phi` storage from
bounded per-value block masks: values with disjoint live regions reuse a
four-byte slot. Incoming `phi` assignments have parallel-copy semantics. The
edge scheduler first emits copies whose destinations are no longer needed as
sources; if the remaining graph is a cycle, it preserves one old destination
in a single reserved four-byte temporary and completes the cycle with `D7` as
the saved scratch register. The temporary is omitted when no edge needs it.
The nested conditional corpus contains six live `phi` values but needs only two
slots; the loop corpus additionally verifies a real exchange cycle.

`break` and `continue` remain pending; their future lowering must funnel all
loop-back control through the canonical latch and all exits through the
canonical exit block rather than expose multiple backedges or exit targets to
the optimizer. Declarations or returns inside control-flow bodies, copy
propagation across joins, address-register allocation, and the shared
low-level m68k model also remain pending. Global value numbering, expensive
interprocedural optimization, speculative dynamic typing, and exhaustive
instruction scheduling remain excluded until measurements justify their
compiler cost.

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
  removal, instruction selection, CFG-aware liveness/allocation, `phi`-slot
  coloring, and immediate-form selection.
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

The first optimization milestone is met. Across six reviewed source corpora
and six edge inputs each, both optimization levels agree with the typed IR
under Musashi. The current regression measurements are shown as code bytes /
executed instructions / maximum callee stack bytes:

| Corpus | `-O0` | `-O1` |
| --- | ---: | ---: |
| arithmetic and parameter reuse | 116 / 41 / 28 | 28 / 12 / 4 |
| folding, negation, and identities | 132 / 46 / 28 | 28 / 10 / 4 |
| register pressure and repeated products | 120 / 43 / 32 | 40 / 13 / 8 |
| typed locals, reassignment, and dead store | 160 / 53 / 32 | 28 / 12 / 4 |
| six comparisons and nested `if`/`else` | 548 / 120-122 / 32 | 356 / 60-62 / 24 |
| `while`, loop-carried values, nested `if`, copy cycle | 308 / 313 / 48 | 188 / 164 / 28 |

The register-pressure corpus forces simultaneous `D3/D4` allocation and
verifies their ABI preservation. A separate deliberately pressure-heavy value
IR fixture forces three reusable spill slots; its 96-byte image executes 33
instructions with 36 callee stack bytes and agrees with the source-level oracle
for six edge inputs. This brings the current Musashi compiler total to 78
executions.

The `-Os` 68020/libnix compiler currently has a 47,272-byte linked
text/data/BSS footprint (46,872 text, 280 data, 120 BSS). Its host and Amiga
builds emit byte-identical ordinary, local-heavy, conditional, loop, and
spilling `-O1` assembly. Unconditional jumps to the physically next block are
elided at both optimization levels. This lets the dedicated latch normalize
the IR without adding an extra runtime branch per iteration, and also removes
other redundant fall-through jumps. The conditional corpus reduces code by
35% and executed instructions by about 49-50% at O1. CFG-aware reuse and
`phi` coloring reduce its maximum callee stack from the previous 44 bytes to 24
bytes, below O0's 32 bytes. On the loop corpus, O1 reduces code by 38%, executed
instructions by 48%, and maximum callee stack use from 48 to 28 bytes. These
counts are regression signals, not A1200 cycle claims.
