# MIGA Lua Compiler Bootstrap

**Status:** initial typed-expression path implemented

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

The implementation has three bounded, host-buildable layers:

1. The frontend produces an `i32` AST in a fixed 128-node arena and reports the
   first error with a one-based line and column.
2. Lowering produces a typed stack IR. A host interpreter for this IR is the
   semantic oracle and uses unsigned C operations to specify 32-bit wrapping.
3. The development backend renders GNU m68k assembly. Parameters are copied to
   fixed `A6` frame slots, expression temporaries use the generated-code stack,
   and the epilogue restores `A6` and `A7` before `RTS`.

For the current local toolchain, GNU `m68k-amigaos-as` retains a relocatable
Amiga object and `m68k-amigaos-objcopy` extracts the flat image consumed by
Musashi. ELF linking, symbol-manifest loading, the shared low-level instruction
model, and the shipping direct encoder remain later steps. The current
stack-heavy renderer is a correctness oracle rather than the shipping
allocation strategy; see the [MIGA Lua Optimization
Strategy](./MIGA-Lua-optimization-strategy.md).

## Commands

Build the compiler and render assembly:

```sh
gmake miga80c
build/host/miga80c/miga80c tests/compile/arithmetic.lua -S -o /tmp/arithmetic.s
```

Evaluate the same typed IR on the host:

```sh
build/host/miga80c/miga80c tests/compile/arithmetic.lua --eval 7 5 2
```

Run native frontend/IR tests and the complete differential path:

```sh
gmake compiler-abi-test compiler-test compiler-execute-test
```

Cross-build this same C99 compiler bootstrap for 68020/libnix and execute its
typed-IR evaluator under `vamos`:

```sh
gmake compiler-amiga-test
```

This proves that the bounded frontend and IR core already run as an Amiga
program. It does not yet prove on-Amiga direct machine-code emission; the
shipping encoder and instruction-cache synchronization remain later work.

The differential test preserves generated assembly, relocatable object, and
flat binary under `build/host/compiler-pipeline/`. Six edge cases must produce
the same `D0` value in the typed-IR interpreter and Musashi, while the runner
also verifies return, stack, callee-saved registers, memory guards, and the
instruction budget.
