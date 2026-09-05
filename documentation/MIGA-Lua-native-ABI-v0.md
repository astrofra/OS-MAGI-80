# MIGA Lua Native ABI 0.4

**Status:** frozen bootstrap exact-width scalar, immutable string/symbol,
register, stack, and controlled-fault core

This is the private calling convention between generated MIGA Lua functions
and the trusted MIGA-80 runtime. It is not the Amiga C ABI, an AmigaOS ABI, or
part of the source-cartridge format. Version 0.4 freezes the rules needed by
the compiler bootstrap, exact-width integer normalization, immutable literal
pool, register allocator, and first source-located runtime fault.

## Target and value representation

- Generated instructions target user-mode 68EC020/68020 without an FPU or MMU.
- `i8`, `u8`, `i16`, `u16`, and `i32` occupy one 32-bit data register or one
  four-byte aligned scalar slot at this ABI boundary.
- A stored or transferred `i8`/`i16` value is sign-extended to 32 bits. A stored
  or transferred `u8`/`u16` value is zero-extended. Non-canonical high bits are
  invalid on function entry and may not cross a generated call boundary.
- Ordinary integer arithmetic wraps modulo 2^8, 2^16, or 2^32 according to the
  declared type, then restores that canonical sign/zero extension.
- Signed integer division truncates toward zero. A type's minimum value divided
  by `-1` wraps to that minimum value. Unsigned types use unsigned division;
  division by zero is a controlled fault for both forms.
- `bool` occupies a 32-bit scalar: false is exactly zero and true is exactly
  one. Other values are not valid stored Boolean values.
- `symbol` occupies a 32-bit scalar. Zero is invalid; nonzero values are opaque,
  collision-free IDs assigned by the cartridge-wide immutable pool. They are
  not exposed as language integers.
- `string` occupies one 32-bit address value pointing to its immutable inline
  descriptor, defined below.
- The condition-code register is caller-saved and carries no source-language
  value across a call.

The four-byte scalar-slot rule does not freeze in-memory aggregate layout.
Future arrays and records may pack `i8`/`u8` elements into bytes and
`i16`/`u16` elements into words, with explicit extension on load; that layout
will receive its own versioned contract.

## Register contract

| Register | ABI 0.4 role | Preservation |
|---|---|---|
| `D0-D2` | First three scalar arguments; `D0` scalar result | Caller-saved |
| `D3-D7` | Allocatable scalar values | Callee-saved |
| `A0-A1` | First two address-class arguments; `A0` address result | Caller-saved |
| `A2-A4` | Allocatable address values | Callee-saved |
| `A5` | Immutable runtime-context pointer | Callee-saved and reserved |
| `A6` | Optional frame pointer | Callee-saved |
| `A7` | Stack pointer | Special; restored by the call/return sequence |

Arguments are assigned from left to right within their type class. Mixed
scalar/address signatures therefore advance the two register sequences
independently. `i8`, `u8`, `i16`, `u16`, `i32`, `bool`, and `symbol` use the
scalar class; `string` uses the address class. A signature may therefore have
at most three scalar and two address parameters in any source order.
There are no stack arguments in ABI 0.4. A signature that exhausts either
register class must be rejected until a later ABI version defines aggregates
and stack argument placement.

Generated code preserves `A5` and may read its first long word at offset zero,
which ABI 0.2 introduced as the non-null controlled-fault handler address.
ABI 0.4 retains it unchanged. Remaining
runtime-context fields and the service jump table require later versioned
extensions. Addresses used by the Musashi harness are test configuration and
are not ABI constants.

## Stack and frame contract

- The stack grows toward lower addresses and has no red zone.
- `A7` is four-byte aligned immediately before a call and on callee entry. A
  68020 `BSR` or `JSR` pushes a four-byte return PC and retains this alignment.
- On entry, `(A7)` is the return PC. A framed function uses
  `link.w A6,#-frame_size`; saved `A6` is then at `0(A6)` and the return PC at
  `4(A6)`.
- Frame size is a multiple of four and at most 32,768 bytes in ABI 0.4. Locals
  and spills use negative offsets from `A6`.
- A leaf may omit the frame and leave `A6` untouched.
- Before `RTS`, a function restores every callee-saved register and restores
  `A7` to its entry value. After `RTS`, the caller observes its pre-call `A7`.

The shipping runtime will enter generated code on a dedicated guarded stack.
Its trampoline, static call-depth calculation, overflow protocol, stop checks,
and context layout are not implied by the synthetic Musashi stack addresses.

## Calls and returns

A caller may not retain live values in `D0-D2`, `A0-A1`, or the condition codes
across a call. A callee may use those resources without saving them. `D3-D7`,
`A2-A5`, and a used `A6` must be restored exactly. A scalar function returns
one value in `D0`; a `string` function returns its descriptor address in `A0`;
a `void` function has no register result.

Multiple returns are excluded from language version 1. Address values other
than immutable strings, stack arguments, varargs, general runtime-service IDs,
and ordinary calls remain unavailable and require an explicit compatible
extension or ABI version bump before compiler code may emit them.

## Controlled fault contract

The long word at `0(A5)` is the address of a trusted, non-returning runtime
fault handler. Generated code enters it with:

| Register | Fault input |
|---|---|
| `D0` | Stable fault code; `1` means integer division by zero |
| `D1` | One-based source line |
| `D2` | One-based source column |
| `A0` | Scratch register holding the handler address |

A function with a dynamic divisor tests it before `DIVS.L` or `DIVU.L`. Its cold fault
site loads `D1` and `D2`, joins a shared function-local tail that loads `D0`,
then performs `movea.l 0(A5),A0` and `jmp (A0)`. It never relies on the CPU's
native divide-by-zero exception. A divisor proven to be a nonzero constant has
no guard or fault site; a divisor proven to be zero is rejected by the
compiler.

The handler does not return through the generated function's active frame. The
shipping runtime trampoline will restore its saved host stack and transfer
control back to MIGA-80. That trampoline implementation remains pending, but
its generated-code entry contract is fixed here. The Musashi harness models it
with a private sentinel address; that address is test configuration, not ABI.

## Immutable `string` and `symbol` ABI

A `string` is an immutable byte sequence, not a NUL-terminated C string. Its
32-bit value points to a four-byte-aligned inline descriptor in read-only
cartridge storage:

```text
offset +0  u32 byte_length, big-endian
offset +4  byte payload[byte_length]
           padding before the next aligned descriptor, if needed
```

The payload may contain zero and is never mutated. The compiler emits literal
addresses with PC-relative `LEA`, so extracting the generated `.text` as a flat
image leaves no literal relocation behind. Equal decoded literals are
deduplicated. The cartridge pack/link step must extend that canonicalization
across separately compiled functions: every equal live string value in one
cartridge must have the same descriptor address. Consequently `==`, `~=`, and
`!=` are constant-time pointer comparisons, not byte loops. Passing a distinct
descriptor with equal bytes violates this canonical-value contract.

A `symbol` is an opaque nonzero 32-bit scalar ID interned from a compile-time
string. IDs are collision-free identities, not hashes; equality is one 32-bit
comparison and ordering/arithmetic are invalid. The bootstrap's single-function
unit assigns IDs in first-occurrence order among symbol literals. The future
cartridge pack/link step must merge names and rewrite IDs deterministically
before multiple compilation units can call one another. Symbol spelling is not
part of the runtime value.

Source literals accept single or double quotes and the escapes `\\`, `\'`,
`\"`, `\n`, `\r`, `\t`, `\0`, and `\xNN`. `symbol("name")` is the explicit
compile-time constructor. There is no implicit or explicit runtime conversion
between `string` and `symbol` in ABI 0.4.

The current bounded compiler arena permits at most 32 deduplicated entries and
1,024 decoded payload bytes per function. Those are bootstrap compiler limits,
not descriptor-field limits; changing them does not change the ABI layout.

## Executable conformance

`compiler/abi/abi.h` and `compiler/abi/abi.c` are the machine-readable source
of this register contract. The GNU assembly renderer obtains argument register
names from it. The Musashi runner obtains its callee-saved set from it, gives
`A5` a valid synthetic context address, checks four-byte stack alignment,
measures maximum callee stack use, and uses a negative control to prove that a
modified saved register is detected. The `-O0` typed-local corpus exercises a
20-byte `A6` frame with parameter slots followed by source-local slots; its
optimized counterpart erases those slots when values remain in registers. The
generated spill fixture additionally exercises a 12-byte `A6` frame, direct
negative-offset reloads, and preservation of `D3-D7/A6` across six edge
inputs. The conditional corpus exercises canonical Boolean arguments and
results, all six integer comparisons, conditional branches, nested CFG joins,
and CFG-aware coalesced `phi` edge slots. The loop corpus exercises backward
branches through a dedicated single latch, one exit target, loop-carried `phi`
values, and parallel edge transfers. Fall-through jump elision prevents that
normalized shape from adding a redundant branch per iteration. A genuine
two-value exchange on the latch proves that the backend preserves one old
value in a four-byte temporary before rewriting the cyclic destinations; the
optimized case uses a 20-byte `A6` frame and reaches 28 bytes of callee stack
including saved registers and the linked frame. The separate loop-control
corpus verifies binary `break`/`continue` funnels and reaches 32 bytes at both
optimization levels. The signed-division corpus verifies native `DIVS.L`,
truncation toward zero, wrapping `INT32_MIN / -1`, guard removal for known
nonzero divisors, and two source-located division-by-zero faults through the
`A5` handler contract.
The exact-width corpus additionally verifies canonical signed and unsigned
arguments/results, per-width wraparound, `DIVS.L` versus `DIVU.L`, signed versus
unsigned condition codes, and source-located zero-divisor faults at both
optimization levels.
The immutable-value corpus verifies decoded-literal deduplication, `string` and
`symbol` joins, pointer/ID equality, PC-relative pool loads with no object
relocations, and both paths under Musashi at `-O0` and `-O1`. Mixed signatures
and an address result are additionally checked in the host backend tests.

Run the host contract and generated-code checks with:

```sh
gmake compiler-abi-test miga68k-test compiler-execute-test compiler-spill-test
```
