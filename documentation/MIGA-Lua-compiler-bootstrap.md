# MIGA Lua Compiler Bootstrap

**Status:** typed locals, entry basic block, `-O1` value backend, and spills implemented

## Scope

The bootstrap compiler proves the complete host development path without
claiming the version 1.0 grammar is frozen. Its accepted source is exactly one
public `i32` function:

```ebnf
function   = "function", name, "(", [ parameters ], ")", ":", "i32",
             { statement }, return-statement, "end" ;
parameters = parameter, { ",", parameter } ;
parameter  = name, ":", "i32" ;
statement  = local-declaration | assignment ;
local-declaration = "local", name, ":", "i32", "=", expression ;
assignment = local-name, "=", expression ;
return-statement = "return", expression ;
expression = product, { ( "+" | "-" ), product } ;
product    = unary, { "*", unary } ;
unary      = "-", unary | integer | parameter-name | local-name
             | "(", expression, ")" ;
```

Whitespace and Lua line comments beginning with `--` are accepted. Decimal
literals are limited to `0` through `2147483647`. A function has at most 16
function-scoped `i32` locals and 32 statements including the final return.
Declarations require an initializer; a local is visible only after that
initializer, cannot shadow a parameter or another local, and parameters are
immutable. The initial ABI supports at most three parameters in `D0` through
`D2`, with the `i32` result in `D0`. Arithmetic has defined two's-complement
wrapping semantics. Register and frame placement follows [MIGA Lua Native ABI
0.1](./MIGA-Lua-native-ABI-v0.md).

Everything else—including calls, control flow, other types, multiple
functions, hexadecimal source literals, and the minimum `i32` literal
spelling—is rejected rather than guessed.

## Pipeline

The implementation has four bounded, host-buildable layers:

1. The frontend produces an `i32` AST with bounded node, statement, and local
   tables and reports the first error with a one-based line and column.
2. Lowering produces a typed stack IR with explicit local loads/stores and one
   entry basic block. Blocks already carry bounded successor slots for the
   future `if` and `while` lowering. The current interpreter and backends
   deliberately reject multi-block IR until their control-flow handling lands.
   A host interpreter is the semantic oracle and uses unsigned C operations to
   specify 32-bit wrapping.
3. `-O1` renames straight-line locals to their current value while lowering to
   value IR. It then folds and simplifies constants, removes dead values and
   overwritten assignments, computes liveness, linearly allocates `D0-D7`, and
   replans with bounded spill slots when register pressure requires it. Calls
   will need an explicit side-effect rule before this renaming crosses them.
4. The development backend renders GNU m68k assembly. `-O0` retains fixed `A6`
   parameter/local slots and expression-stack temporaries as a baseline. The
   default `-O1` keeps current local and expression values in registers and
   preserves any allocated `D3-D7` registers with `MOVEM`. Spilling functions
   use ABI 0.1 `LINK`/`UNLK` frames, negative `A6` offsets, and `D7` as a saved
   scratch register.

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
flat binaries under the compiler pipeline build directories. Four source
corpora—including typed local declarations, reassignment, and a dead store—use
six edge inputs each at both levels. Those 48 executions must produce the same
`D0` value as the typed-IR interpreter. A synthetic value-IR schedule then
forces three spills and adds six more oracle comparisons, bringing the total
to 54. This is necessary because the current bounded source subset cannot
naturally exceed all eight data registers. The reports retain image size,
executed instruction count, and maximum callee stack use, while the runner
verifies return, stack balance, callee-saved registers including `A6`, memory
guards, and the instruction budget.
