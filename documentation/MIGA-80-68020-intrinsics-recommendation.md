# MIGA-80 68020 Intrinsics Recommendation

**Status:** design recommendation  
**Target:** statically typed MIGA Lua compiled ahead of time for a stock Amiga 1200  
**CPU:** 68EC020  
**Primary constraint:** the compiler, editor, runtime, cartridge source and user program must coexist on a vanilla machine

---

## 1. Purpose

MIGA Lua should offer a small set of low-level operations that are efficient on the 68020 without exposing physical registers or embedding arbitrary assembly.

These operations are called **intrinsics**. They look like ordinary typed functions or operators in source code, but the compiler understands their exact semantics and may lower them directly to one instruction, a short instruction sequence, a constant, or a runtime call.

The goal is not to turn MIGA Lua into an assembler. The goal is to expose a carefully selected set of operations useful for:

- graphics and packed-pixel manipulation;
- fixed-point mathematics;
- compression and decompression;
- pseudo-random generators and hashes;
- tiles, sprites and object attributes;
- controlled access to cartridge memory;
- performance-critical inner loops.

---

## 2. Core design principle

Intrinsics must describe **what an operation means**, not which register or exact instruction must be used.

For example:

```lua
local result: u32 = swap16(value)
```

has the following language-level meaning:

```text
0xAAAABBBB -> 0xBBBBAAAA
```

On the 68020, the natural lowering is usually:

```asm
SWAP Dn
```

However, the source program does not require a specific register and does not force the compiler to emit `SWAP`. The optimizer remains free to:

- evaluate the operation at compile time;
- eliminate two consecutive swaps;
- reuse a value already available in the required form;
- select a different implementation on another backend;
- move the operand into a temporary data register when necessary.

This separation preserves register allocation, spilling, calling conventions and future optimization.

---

## 3. Admission criteria

An intrinsic should be added only when it satisfies at least one of these conditions:

1. The operation is impossible or awkward to express using ordinary MIGA Lua.
2. It maps particularly well to the 68020.
3. Recognizing the operation enables a significant optimization.
4. It communicates intent more clearly than a long expression.
5. It avoids introducing a larger or more complex source-language feature, such as general `i64` arithmetic.
6. It is useful across several cartridges or runtime subsystems rather than for one isolated effect.

Every intrinsic adds work to the parser or symbol table, type checker, constant evaluator, IR, optimizer, backend, host semantic oracle, Musashi tests and user documentation. The initial set must therefore remain deliberately small.

---

## 4. General semantic rules

### 4.1 Intrinsics are reserved built-ins

Intrinsic names are resolved by the compiler and cannot be redefined by cartridge code.

```lua
local y: u32 = swap16(x)
```

They should initially use ordinary call syntax. This avoids adding a special syntactic category to the parser.

### 4.2 Evaluation order

Arguments are evaluated from left to right, once each, before the intrinsic operation is performed.

### 4.3 Integer widths

The exact-width types used in this document are:

| Type | Meaning |
|---|---|
| `bool` | Boolean value |
| `u8` | Unsigned 8-bit integer |
| `u16` | Unsigned 16-bit integer |
| `u32` | Unsigned 32-bit integer |
| `i32` | Signed 32-bit integer with wrapping two's-complement arithmetic |
| `fix16` | Proposed signed 16.16 fixed-point value represented in 32 bits |

If MIGA Lua initially implements only `i32`, the intrinsic rollout should wait for the minimum unsigned types required to make bit operations unambiguous. Bit-pattern operations are clearer and safer on `u32` than on a signed value.

### 4.4 Shift and rotation counts

The language must define counts independently from incidental host-language or CPU behavior. The recommended rule for 32-bit operations is:

```text
effective count = count & 31
```

This makes all input values deterministic and permits constant folding to match generated code exactly.

### 4.5 No observable condition codes

The 68020 condition-code register is not part of MIGA Lua program state. An intrinsic may affect hardware flags internally, but source code cannot inspect or preserve them directly.

### 4.6 Constant evaluation

Every pure intrinsic must be evaluated by the host semantic oracle and constant folder. For example:

```lua
local value: u32 = swap16(0x11223344)
```

should compile as the constant `0x33441122`, with no runtime operation.

---

## 5. Recommended version 1 intrinsic set

The first version should contain only operations with high utility, simple semantics and straightforward validation.

### 5.1 `swap16`

```lua
swap16(value: u32): u32
```

Exchanges the upper and lower 16-bit words of a 32-bit value.

```text
swap16(0x11223344) = 0x33441122
swap16(swap16(x)) = x
```

Probable 68020 lowering:

```asm
SWAP Dn
```

Typical uses:

- exchanging two packed 16-bit values;
- packed-pixel and planar conversion code;
- decompression;
- rearranging fixed-width binary fields;
- loading two words in an order convenient for later operations.

The name must not be confused with complete byte reversal:

```text
swap16(0x11223344) != 0x44332211
```

### 5.2 `rol`

```lua
rol(value: u32, count: u32): u32
```

Rotates a 32-bit value to the left. Bits leaving bit 31 re-enter at bit 0.

```text
rol(0x80000001, 1) = 0x00000003
rol(x, 0) = x
```

Probable lowering:

```asm
ROL.L #count,Dn
```

or a register-count form when the count is dynamic.

Typical uses:

- pseudo-random generators;
- hashing and checksums;
- packed graphics;
- compression;
- procedural effects.

### 5.3 `ror`

```lua
ror(value: u32, count: u32): u32
```

Rotates a 32-bit value to the right. Bits leaving bit 0 re-enter at bit 31.

```text
ror(0x80000001, 1) = 0xC0000000
ror(x, 0) = x
```

Probable lowering:

```asm
ROR.L #count,Dn
```

The compiler may canonicalize one rotation direction into the other when profitable.

### 5.4 Fixed-point multiplication

```lua
fix_mul(a: fix16, b: fix16): fix16
```

Computes the signed 16.16 result:

```text
(a * b) >> 16
```

The specification must freeze:

- signed behavior;
- intermediate precision;
- rounding direction;
- overflow behavior;
- the result of extreme values.

Recommended initial rule:

- use a mathematically signed 64-bit intermediate;
- shift right by 16 with truncation toward negative infinity if defined as an arithmetic shift, or explicitly define truncation toward zero;
- retain the low 32 bits of the shifted result, giving deterministic wrapping semantics.

The exact choice matters more than the choice itself: the native semantic oracle and 68020 backend must agree for every input.

A 68020 backend may use long multiply instructions and a register pair without exposing `i64` to source programs.

Typical uses:

- 2D transformations;
- 3D transformations;
- camera movement;
- interpolation;
- sprite scaling calculations;
- physics and procedural animation.

### 5.5 Fixed-point division

```lua
fix_div(a: fix16, b: fix16): fix16
```

Computes a defined equivalent of:

```text
(a << 16) / b
```

The specification must define division by zero, overflow and rounding. The recommended failure policy is a deterministic runtime trap in checked builds and the same defined trap in release builds, rather than inheriting undocumented CPU exception behavior.

Division is expensive, but expressing it as an intrinsic allows the compiler to:

- fold constant cases;
- replace division by powers of two;
- select a runtime implementation;
- use a native sequence where appropriate.

### 5.6 Controlled cartridge-memory access

```lua
peek8(address: u32): u8
peek16(address: u32): u16
peek32(address: u32): u32

poke8(address: u32, value: u8): void
poke16(address: u32, value: u16): void
poke32(address: u32, value: u32): void
```

These operations access the MIGA-80 **virtual cartridge address space**, not unrestricted Amiga memory.

Required semantics:

- multi-byte values use one documented byte order;
- valid address ranges are defined by the cartridge memory map;
- out-of-range accesses trap deterministically;
- word and longword alignment rules are explicit;
- direct access to MIGA-80 runtime internals and Amiga custom registers is forbidden;
- volatile behavior is not implied unless a future virtual-device region explicitly requires it.

Recommended byte order: big-endian, matching the 68020 and simplifying native aligned loads and stores.

Typical uses:

- binary asset decoding;
- compact arrays and packed structures;
- user-defined allocators;
- decompression buffers;
- save-state serialization inside a controlled memory region.

These functions should be postponed until the virtual memory model is stable. They are included in the recommended v1 surface only if cartridges genuinely need address-oriented access; otherwise typed arrays are safer and easier to optimize.

---

## 6. Operators that should not be function intrinsics

Common bitwise operations should use ordinary operators because they are fundamental language expressions rather than exceptional hardware facilities.

```lua
local masked: u32 = value & 0x00ff00ff
local merged: u32 = a | b
local toggled: u32 = value ~ mask
local inverted: u32 = ~value
local left: u32 = value << 4
local right: u32 = value >> 4
```

Recommended typing rules:

| Expression | Meaning |
|---|---|
| `u32 >> n` | Logical right shift |
| `i32 >> n` | Arithmetic right shift |
| `x << n` | Left shift with discarded high bits |
| `&`, `|`, `~` | Width-preserving bit operations |

The use of `~` for both binary exclusive-or and unary complement follows Lua's established bitwise syntax, if compatibility with the chosen Lua subset is desired.

The optimizer may select `BTST`, `BSET`, `BCLR` or `BCHG` when ordinary expressions make such a transformation valid.

---

## 7. Recommended version 2 candidates

These operations are useful but should not burden the initial compiler until real code demonstrates a need.

### 7.1 Individual bit operations

```lua
bit_test(value: u32, bit: u32): bool
bit_set(value: u32, bit: u32): u32
bit_clear(value: u32, bit: u32): u32
bit_toggle(value: u32, bit: u32): u32
```

Candidate 68020 instructions:

```asm
BTST
BSET
BCLR
BCHG
```

Recommended index rule for `u32`: `bit & 31`.

These functions can improve readability, but most can already be represented with operators. Add them only if they produce clearer cartridge code or enable reliable backend optimizations.

### 7.2 Bit-field extraction

```lua
bit_extract(value: u32, offset: u32, width: u32): u32
bit_extract_signed(value: u32, offset: u32, width: u32): i32
```

Candidate 68020 instructions:

```asm
BFEXTU
BFEXTS
```

Potential uses:

- tile attributes;
- sprite flags;
- packed colors;
- compressed command streams;
- object identifiers and state fields.

The source-level bit numbering must be defined explicitly. It must not merely say “whatever `BFEXTU` does.” The specification should state which bit is offset zero and how fields crossing word boundaries behave.

### 7.3 Bit-field insertion

```lua
bit_insert(base: u32, field: u32, offset: u32, width: u32): u32
```

Candidate instruction:

```asm
BFINS
```

Only the lowest `width` bits of `field` are inserted. All other bits of `base` remain unchanged.

This must remain a pure value operation. It should not secretly modify its first argument or arbitrary memory.

### 7.4 Byte reversal

```lua
bswap32(value: u32): u32
```

```text
bswap32(0x11223344) = 0x44332211
```

The 68020 does not provide a direct 32-bit byte-swap instruction equivalent to later architectures. The backend would emit a short sequence. Add this intrinsic only if little-endian assets or protocols make it common.

### 7.5 Bit counting

```lua
popcount(value: u32): u32
count_leading_zeros(value: u32): u32
count_trailing_zeros(value: u32): u32
```

These operations do not necessarily map to one ideal 68020 instruction. They can still be valuable because the compiler may use specialized sequences, lookup tables or runtime helpers.

They should be introduced only after measurement on representative cartridges.

### 7.6 High-half multiplication

```lua
mul_high_signed(a: i32, b: i32): i32
mul_high_unsigned(a: u32, b: u32): u32
```

These return the upper 32 bits of a full 64-bit product. They may support fixed-point and numerical kernels without introducing a public 64-bit type.

Prefer domain-level `fix_mul()` in ordinary cartridge code. Add high-half multiplication only for authors implementing custom numerical formats or low-level algorithms.

---

## 8. Variable exchange is not the 68k `SWAP` operation

The 68k `SWAP` instruction exchanges the two 16-bit halves of one data register. It does not exchange two variables.

Variable exchange should initially remain ordinary source code:

```lua
local temporary: i32 = a
a = b
b = temporary
```

The value IR and register allocator can normally remove unnecessary memory traffic.

Do not add multiple assignment merely to support swapping:

```lua
-- Not required in the initial language
a, b = b, a
```

Lua-style multiple assignment would affect parsing, semantic rules, temporaries, calls and potentially multiple return values. Its cost is disproportionate to this single use case.

---

## 9. Hardware operations belong to the virtual machine API

Graphics, audio and timing operations may also compile efficiently, but they should be specified as MIGA-80 virtual-machine services rather than generic CPU intrinsics.

Examples:

```lua
sprite_set(slot, tile, x, y, flags)
layer_scroll(layer, x, y)
pixel_set(x, y, color)
map_set(x, y, tile)
sound_play(channel, sound)
```

The compiler may inline or specialize these calls later, but their semantics belong to the fantasy hardware. This distinction prevents CPU utilities and machine services from becoming one incoherent namespace.

Suggested conceptual separation:

```text
CPU/value intrinsics
  swap16, rol, ror, fix_mul

Controlled memory operations
  peek8, peek16, peek32, poke8, poke16, poke32

Virtual hardware API
  sprite_set, layer_scroll, map_set, sound_play
```

---

## 10. Features explicitly excluded

### 10.1 Forced registers

The language must not allow:

```lua
register d0: i32
force_register(value, "d3")
```

Forced registers would interfere with:

- value-IR scheduling;
- liveness analysis;
- register allocation;
- spilling;
- function calls;
- callee-saved registers;
- ABI evolution;
- optimization across intrinsic boundaries.

### 10.2 Inline assembly

The language must not allow:

```lua
asm("swap d0")
```

Inline assembly would require the compiler to understand clobbers, inputs, outputs, memory effects, condition codes, control flow and register constraints. A deliberately simplified form would be unsafe or would silently miscompile optimized programs.

Runtime developers can still write isolated assembly modules behind stable internal APIs. Cartridge authors should not embed arbitrary 68k code.

### 10.3 Direct custom-register access

The following style must not be supported by cartridge code:

```lua
poke16(0xDFF100, value)
```

It would couple cartridges directly to AGA, bypass resource ownership, permit system corruption and prevent alternative implementations of MIGA-80.

### 10.4 Observable stack or memory addresses for locals

An intrinsic must not expose the physical address of a local value. The compiler must remain free to keep locals in registers, merge them, eliminate them or spill them to different frame slots.

### 10.5 Instruction-shaped aliases without semantic value

Do not create one builtin for every 68020 mnemonic. Operations such as `MOVE`, `ADD`, `SUB`, `CMP` and ordinary shifts already belong to expressions and assignments. An intrinsic collection mirroring the instruction manual would duplicate the language and constrain optimization.

---

## 11. Compiler representation

Intrinsics should be recognized during semantic analysis and lowered to dedicated typed IR operations rather than remaining ordinary unresolved calls.

Example:

```text
Source:
  local y: u32 = swap16(x)

Typed AST:
  CALL_INTRINSIC SWAP16 : u32
    LOCAL x : u32

Value IR:
  v2:u32 = swap16 v1:u32

68020 selection:
  v1 and v2 may share Dn
  SWAP Dn
```

Suggested IR identifiers:

```text
MIGA80_VALUE_SWAP16_U32
MIGA80_VALUE_ROL_U32
MIGA80_VALUE_ROR_U32
MIGA80_VALUE_FIX_MUL_16_16
MIGA80_VALUE_FIX_DIV_16_16
```

Memory operations must be explicitly marked as side-effecting so that dead-code elimination, common-subexpression elimination and instruction reordering cannot remove or incorrectly move them.

Pure value intrinsics such as `swap16`, `rol`, `ror` and `fix_mul` may participate normally in constant folding and dead-value elimination.

---

## 12. Register allocation implications

Some 68020 instructions require data-register operands. This is a backend constraint, not a source-language constraint.

For:

```lua
local y: u32 = swap16(x)
```

the allocator or instruction selector may:

1. keep `x` in a data register and apply `SWAP` in place;
2. copy `x` to a free data register if the original value remains live;
3. reload a spilled `x` into a scratch data register;
4. fold or eliminate the operation before allocation.

The intrinsic definition should record relevant machine constraints for instruction selection, but the user-facing type system should not acquire “data-register values” or “address-register values.”

---

## 13. Optimization opportunities

The following identities are safe examples for unsigned 32-bit operations:

```text
swap16(swap16(x)) = x
rol(x, 0) = x
ror(x, 0) = x
rol(x, n) = ror(x, 32 - (n & 31))
ror(x, n) = rol(x, 32 - (n & 31))
rol(rol(x, a), b) = rol(x, a + b)
ror(ror(x, a), b) = ror(x, a + b)
```

Additional profitable cases:

- evaluate all-constant arguments at compile time;
- replace `fix_mul(x, 0)` with zero;
- replace `fix_mul(x, 1.0)` with `x`;
- replace fixed-point multiplication or division by powers of two with defined shifts when rounding semantics permit it;
- combine masks with bit extraction;
- select immediate instruction forms for constant counts;
- eliminate pure intrinsic results that are never used.

Optimizations must preserve the frozen language semantics, especially signed rounding and overflow behavior.

---

## 14. Validation requirements

Each intrinsic requires four matching implementations or test layers:

1. a specification-level reference implementation on the host;
2. constant evaluation in the compiler;
3. value-IR interpretation for the semantic oracle;
4. 68020 lowering executed under Musashi.

Selected cases must later be verified under `vamos`, UAE and a physical A1200 where relevant.

### Required test classes

- zero values;
- all-one values;
- alternating bit patterns;
- minimum and maximum signed values;
- every constant shift/rotation count from 0 through 31;
- dynamic counts including values above 31;
- aliasing and liveness around destructive instructions;
- spilled operands;
- constant folding;
- dead-result elimination;
- interaction with calls and callee-saved registers;
- invalid types and arity;
- memory boundaries and misalignment;
- division by zero and fixed-point overflow.

Example `swap16` corpus:

```text
0x00000000 -> 0x00000000
0xFFFFFFFF -> 0xFFFFFFFF
0x11223344 -> 0x33441122
0x0000FFFF -> 0xFFFF0000
0x80000001 -> 0x00018000
```

Differential execution must compare the host semantic oracle with generated 68020 code, not merely compare generated assembly text.

---

## 15. Diagnostics

Diagnostics should describe the semantic operation rather than backend restrictions.

Good:

```text
error: swap16 expects u32, received i32
error: rol expects 2 arguments, received 1
error: poke32 address is outside cartridge memory
```

Avoid exposing allocation details:

```text
error: SWAP requires a D register
```

If the backend cannot satisfy an instruction constraint, that is a compiler failure, not a cartridge-source error.

---

## 16. Namespacing

For the initial compact language, short global names are acceptable:

```lua
swap16(x)
rol(x, n)
fix_mul(a, b)
```

If the builtin surface grows, explicit namespaces become preferable:

```lua
bits.swap16(x)
bits.rol(x, n)
fixed.mul(a, b)
memory.peek16(address)
```

Namespaces improve discoverability but require field access or qualified-name support in the parser and type checker. Do not add that machinery only for aesthetic reasons. A small flat intrinsic set is appropriate for the bootstrap compiler.

---

## 17. Recommended rollout

### Stage 1 — bitwise foundation

Implement ordinary typed operators:

```text
&, |, ~, <<, >>
```

Freeze signed and unsigned shift semantics.

### Stage 2 — minimal pure intrinsics

Implement:

```lua
swap16(value: u32): u32
rol(value: u32, count: u32): u32
ror(value: u32, count: u32): u32
```

These have simple semantics, no side effects and direct 68020 mappings.

### Stage 3 — fixed-point domain operations

After freezing `fix16`, implement:

```lua
fix_mul(a: fix16, b: fix16): fix16
fix_div(a: fix16, b: fix16): fix16
```

Validate them exhaustively around signs, rounding and overflow boundaries.

### Stage 4 — controlled memory

Only after defining the cartridge memory map, consider:

```lua
peek8, peek16, peek32
poke8, poke16, poke32
```

Prefer typed arrays if they cover the actual use cases.

### Stage 5 — evidence-driven extensions

Add bit-field, bit-counting, byte-reversal or high-half multiplication operations only when representative cartridge code shows a repeated need or a measurable performance benefit.

---

## 18. Final recommendation

MIGA-80 should expose **semantic, typed and optimizable intrinsics**, never physical registers or arbitrary assembly.

The recommended initial surface is:

```lua
-- Ordinary operators
&, |, ~, <<, >>

-- Pure 68020-friendly operations
swap16(value: u32): u32
rol(value: u32, count: u32): u32
ror(value: u32, count: u32): u32

-- Fixed-point operations, after fix16 is frozen
fix_mul(a: fix16, b: fix16): fix16
fix_div(a: fix16, b: fix16): fix16
```

Controlled `peek` and `poke` operations should remain conditional on the final virtual memory model. Bit-field and specialized arithmetic operations belong to a later, evidence-driven stage.

The language should explicitly reject:

```text
inline assembly
forced register allocation
condition-code access
physical stack/local addresses
direct AGA custom-register access
one builtin per 68020 mnemonic
```

This provides enough low-level character for graphics, numerical and compression code while keeping the compiler small enough to run alongside the editor, runtime and cartridge on a stock Amiga 1200.
