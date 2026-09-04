# MIGA Lua Compiler Bootstrap

**Status:** typed-expression path, `-O1` value backend, and spills implemented

## Scope

The bootstrap compiler proves the complete host development path without
claiming the version 1.0 grammar is frozen. Its accepted source is exactly one
public `i32` function:

```ebnf
function   = "function", name, "(", [ parameters ], ")", ":", "i32",
             "return", expression, "end" ;
parameters = parameter, { ",", parameter } ;
parameter  = name, ":", "i32" ;
expression = product, { ( "+" | "-" ), product } ;
product    = unary, { "*", unary } ;
unary      = "-", unary | integer | parameter-name | "(", expression, ")" ;
```

Whitespace and Lua line comments beginning with `--` are accepted. Decimal
literals are limited to `0` through `2147483647`. The initial ABI supports at
most three parameters in `D0` through `D2`, with the `i32` result in `D0`.
Arithmetic has defined two's-complement wrapping semantics. Register and frame
placement follows [MIGA Lua Native ABI 0.1](./MIGA-Lua-native-ABI-v0.md).

Everything else—including locals, assignments, calls, control flow, other
types, multiple functions, hexadecimal source literals, and the minimum `i32`
literal spelling—is rejected rather than guessed.

## Pipeline

The implementation has four bounded, host-buildable layers:

1. The frontend produces an `i32` AST in a fixed 128-node arena and reports the
   first error with a one-based line and column.
2. Lowering produces a typed stack IR. A host interpreter for this IR is the
   semantic oracle and uses unsigned C operations to specify 32-bit wrapping.
3. `-O1` lowers the stack IR into a value IR, folds and simplifies constants,
   removes dead values, computes liveness, linearly allocates `D0-D7`, and
   replans with bounded spill slots when register pressure requires it.
4. The development backend renders GNU m68k assembly. `-O0` retains fixed `A6`
   slots and expression-stack temporaries as a baseline. The default `-O1`
   keeps current expression values in registers and preserves any allocated
   `D3-D7` registers with `MOVEM`. Spilling functions use ABI 0.1
   `LINK`/`UNLK` frames, negative `A6` offsets, and `D7` as a saved scratch
   register.

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
flat binaries under the compiler pipeline build directories. Three expression
corpora use six edge inputs each at both levels. Those 36 executions must
produce the same `D0` value as the typed-IR interpreter. A synthetic value-IR
schedule then forces three spills and adds six more oracle comparisons. This is
necessary because the current 128-node source subset cannot naturally exceed
all eight data registers. The reports retain image size, executed instruction
count, and maximum callee stack use, while the runner verifies return, stack
balance, callee-saved registers including `A6`, memory guards, and the
instruction budget.
