# MIGA Lua Compiler Bootstrap

**Status:** exact-width integers, typed locals, signed/unsigned division,
controlled faults, normalized loops, `break`/`continue`, loop `phi`, `-O1`,
and spills implemented

## Scope

The bootstrap compiler proves the complete host development path without
claiming the version 1.0 grammar is frozen. Its accepted source is exactly one
public scalar function:

```ebnf
function   = "function", name, "(", [ parameters ], ")", ":", value-type,
             { statement }, return-statement, "end" ;
parameters = parameter, { ",", parameter } ;
parameter  = name, ":", value-type ;
value-type = integer-type | "bool" ;
integer-type = "i8" | "u8" | "i16" | "u16" | "i32" ;
statement  = local-declaration | control-statement ;
control-statement = assignment | if-statement | while-statement
                    | loop-control-statement ;
local-declaration = "local", name, ":", value-type, "=", expression ;
assignment = local-name, ( "=" | "/=" ), expression ;
if-statement = "if", expression, "then", { control-statement },
               "else", { control-statement }, "end" ;
while-statement = "while", expression, "do", { control-statement }, "end" ;
loop-control-statement = "break" | "continue" ;
return-statement = "return", expression ;
expression = sum, { ( "==" | "~=" | "!=" | "<" | "<=" | ">" | ">=" ), sum } ;
sum        = product, { ( "+" | "-" ), product } ;
product    = unary, { ( "*" | "/" ), unary } ;
unary      = "-", unary | integer | "true" | "false"
             | parameter-name | local-name
             | "(", expression, ")" ;
```

Whitespace and Lua line comments beginning with `--` are accepted. Decimal
literals are limited to `0` through `2147483647`. A function has at most 16
function-scoped scalar locals and 32 statements including nested branches and
the final return.
Declarations require an initializer; a local is visible only after that
initializer, cannot shadow a parameter or another local, and parameters are
immutable. Types are exact. There is no general implicit conversion. The one
implemented exception adapts an `i32` constant expression to `i8`, `u8`,
`i16`, or `u16` when its final value is representable in the destination type;
an out-of-range constant is rejected. Arithmetic, `/`, and ordered comparisons
require two operands of the same integer type. `==`, `~=`, and its exact alias
`!=` require two operands of the same scalar type; `if` and `while` conditions
require `bool`. `if` currently requires an
explicit `else`. Declarations and returns inside `if` branches or loop bodies
remain rejected until lexical scopes and multiple exit blocks are specified.
`break` and `continue` are valid only within the nearest enclosing `while` and
must terminate their immediate statement list; a following statement remains
valid when it is reached through another branch of an enclosing `if`. The
initial ABI supports at most three
scalar parameters in `D0` through `D2`, with one scalar result in `D0`.
Arithmetic wraps at the declared width. Signed `/` truncates toward zero and
minimum-value divided by `-1` wraps to that minimum value; unsigned `/` uses
ordinary unsigned division. A provably constant zero divisor is a compile-time
error. Every other divisor not proven nonzero is checked before `DIVS.L` or
`DIVU.L` and branches to a controlled, source-located runtime fault when zero.
Register, frame, and fault placement follows
[MIGA Lua Native ABI 0.3](./MIGA-Lua-native-ABI-v0.md).

The version 1 language contract requires an explicit return annotation and
includes `void`, but `void` code generation is not in this bootstrap tranche.
The exact spellings `string` and `symbol` are reserved and recognized as types,
but using either currently produces an explicit immutable-pool/address-ABI
dependency diagnostic. `string` is planned as an immutable byte sequence
backed by a deduplicated constant pool and represented by an address plus a
length. `symbol` is planned as a cartridge-wide interned, opaque 32-bit
identity, with equality but no arithmetic or ordering. The string descriptor,
relocations, literal grammar, interning step, and mixed scalar/address calling
convention must be implemented together in the next data tranche. There are no
`byte` or `word` aliases: the source spellings remain `i8`, `u8`, `i16`, and
`u16`.

Calls, fixed point, multiple functions, multiple returns, hexadecimal
source literals, and the minimum `i32` literal spelling are likewise rejected
rather than guessed.

Arrays are not part of the bootstrap grammar yet. Their frozen version 1
language contract is nevertheless zero-based: for `array<T, N>`, valid indices
are exactly `0` through `N - 1`, and index `0` denotes the first element.

`x /= value` is implemented as statement-only sugar for `x = x / value`; it
cannot appear in an expression or an `if`/`while` condition. Planned sibling
forms are `x++`, `x--`, `x += value`, `x -= value`, and `x *= value`, under the
same statement-only rule.

## Pipeline

The implementation has four bounded, host-buildable layers:

1. The frontend produces a typed scalar AST with bounded node, statement, and local
   tables and reports the first error with a one-based line and column.
2. Lowering produces a typed stack IR with explicit local loads/stores,
   comparisons, conditional/unconditional terminators, and up to 32 basic
   blocks with two successor slots each. A `while` has the canonical shape
   `preheader -> header -> body -> latch -> header`, plus one dedicated exit.
   Every normal/`continue` path is folded through a bounded binary funnel into
   the single latch; the false condition and every `break` path use another
   binary funnel into the single exit. Thus every block still has at most two
   predecessors and binary `phi` values remain sufficient. A `while` whose
   body has no syntactic path back to the header is represented as acyclic and
   needs no unreachable latch. The host interpreter follows this CFG as the
   semantic oracle and uses unsigned C operations to specify per-width
   wrapping, signed/unsigned comparisons, and division without C
   signed-overflow behavior.
3. `-O1` renames locals to values throughout the CFG and creates typed `phi`
   values at two-predecessor joins and loop headers. It identifies natural
   loops with bounded dominator analysis, and validates their single
   preheader, dedicated latch, declared loop region, and unique exit. It creates provisional loop
   `phi` operands before the latch has been lowered, then completes their backward
   inputs and removes trivial self-joins. Constant folding, simplification,
   dead-value removal, and `live-in`/`live-out` analysis all accept cyclic value
   flow. A dynamic division remains live even when its result is overwritten,
   because its zero-divisor fault is observable; division by a proven nonzero
   constant remains removable. `phi` operands are edge-specific uses. Non-overlapping `phi` live
   regions reuse stack slots; edge transfers are scheduled as parallel copies,
   with one bounded temporary slot reserved only when a genuine copy cycle must
   be broken. Calls will need an explicit side-effect rule before value
   renaming crosses them.
4. The development backend renders GNU m68k assembly. `-O0` retains fixed `A6`
   parameter/local slots and expression-stack temporaries as a baseline. The
   default `-O1` keeps current local and expression values in registers and
   preserves any allocated `D3-D7` registers with `MOVEM`. Spilling functions
   use ABI 0.3 `LINK`/`UNLK` frames, negative `A6` offsets, and `D7` as a saved
   scratch register. Both backends omit an unconditional jump when its target
   is the next emitted block, so the dedicated latch does not add a redundant
   branch to the hot loop path. Dynamic divisions add one `TST.L` and one
   normally untaken branch before native `DIVS.L` or `DIVU.L`; cold per-site stubs pass the
   fault code, line, and column to the handler pointer in the `A5` context.

For the current local toolchain, GNU `m68k-amigaos-as` retains a relocatable
Amiga object and `m68k-amigaos-objcopy` extracts the flat image consumed by
Musashi. ELF linking, symbol-manifest loading, the shared low-level instruction
model, and the shipping direct encoder remain later steps. The `-O0`
stack-heavy renderer remains a correctness oracle; see the [MIGA Lua Optimization
Strategy](./MIGA-Lua-optimization-strategy.md).

## Commands

Build the compiler and render assembly:

```sh
gmake miga80c
build/host/miga80c/miga80c tests/compile/arithmetic.lua -S -o /tmp/arithmetic.s
build/host/miga80c/miga80c tests/compile/arithmetic.lua -O0 -S -o /tmp/arithmetic-o0.s
```

Evaluate the same typed IR on the host:

```sh
build/host/miga80c/miga80c tests/compile/arithmetic.lua --eval 7 5 2
```

Run native frontend/IR tests and the complete differential path:

```sh
gmake compiler-abi-test compiler-test compiler-execute-test compiler-spill-test
```

Cross-build this same C99 compiler bootstrap for 68020/libnix and execute its
typed-IR evaluator under `vamos`:

```sh
gmake compiler-amiga-test
```

This proves that the bounded frontend, value optimizer, and assembly renderer
already run as an Amiga program: the target and host builds must render
byte-identical `-O1` assembly. It does not yet prove on-Amiga direct
machine-code emission; the shipping encoder and instruction-cache
synchronization remain later work.

The differential tests preserve `-O0`/`-O1` assembly, relocatable objects, and
flat binaries under the compiler pipeline build directories. Seven ordinary
source corpora—including typed locals, a nested 19-block comparison/branch
fixture, the loop-carried cyclic-copy fixture, and a multi-site `break`/`continue`
fixture—use six edge inputs each at both levels. Those 84 executions must
produce the same `D0` value as the typed-IR interpreter. An eighth signed-
division corpus adds 12 successful results and four controlled zero faults. A
ninth exact-width corpus adds 12 successful `i8`/`u8`/`i16`/`u16` results and
two controlled zero faults, including signed/unsigned comparisons, divisions,
and wrapping normalization. A synthetic value-IR schedule then forces three
spills and adds six more oracle comparisons, bringing the total to 120. This is
necessary because the current bounded source subset cannot naturally exceed
all eight data registers. The reports retain image size, executed instruction
count, and maximum callee stack use, while the runner
verifies return or the expected controlled fault, precise fault location, stack
balance on return, callee-saved registers including `A6`, memory guards, and
the instruction budget.
