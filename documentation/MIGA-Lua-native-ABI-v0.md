# MIGA Lua Native ABI 0.1

**Status:** frozen bootstrap register and stack core

This is the private calling convention between generated MIGA Lua functions
and the trusted MIGA-80 runtime. It is not the Amiga C ABI, an AmigaOS ABI, or
part of the source-cartridge format. Version 0.1 freezes only the rules needed
by the scalar compiler bootstrap and the next register allocator.

## Target and scalar representation

- Generated instructions target user-mode 68EC020/68020 without an FPU or MMU.
- `i32` and `u32` occupy one 32-bit register or one four-byte aligned slot.
- Ordinary integer arithmetic wraps modulo 2^32.
- `bool` occupies a 32-bit scalar: false is exactly zero and true is exactly
  one. Other values are not valid stored Boolean values.
- The condition-code register is caller-saved and carries no source-language
  value across a call.

## Register contract

| Register | ABI 0.1 role | Preservation |
|---|---|---|
| `D0-D2` | First three scalar arguments; `D0` scalar result | Caller-saved |
| `D3-D7` | Allocatable scalar values | Callee-saved |
| `A0-A1` | First two future address-class arguments; `A0` address result | Caller-saved |
| `A2-A4` | Allocatable address values | Callee-saved |
| `A5` | Immutable runtime-context pointer | Callee-saved and reserved |
| `A6` | Optional frame pointer | Callee-saved |
| `A7` | Stack pointer | Special; restored by the call/return sequence |

Arguments are assigned from left to right within their type class. Mixed
scalar/address signatures therefore advance the two register sequences
independently. The 0.1 frontend exposes scalar `i32` and `bool` arguments only;
address arguments are reserved so the register allocator cannot claim their
registers.
There are no stack arguments in ABI 0.1. A signature that exhausts either
register class must be rejected until a later ABI version defines aggregates
and stack argument placement.

Generated code may read `A5` only after the runtime-context layout and service
jump table have their own versioned contract. Until then it is an opaque,
non-null entry value that every generated function must preserve. Addresses
used by the Musashi harness are test configuration and are not ABI constants.

## Stack and frame contract

- The stack grows toward lower addresses and has no red zone.
- `A7` is four-byte aligned immediately before a call and on callee entry. A
  68020 `BSR` or `JSR` pushes a four-byte return PC and retains this alignment.
- On entry, `(A7)` is the return PC. A framed function uses
  `link.w A6,#-frame_size`; saved `A6` is then at `0(A6)` and the return PC at
  `4(A6)`.
- Frame size is a multiple of four and at most 32,768 bytes in ABI 0.1. Locals
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
one value in `D0`; a `void` function has no register result.

Multiple returns are excluded from language version 1. Address values in the
source language, stack arguments, varargs, controlled traps, runtime-service
IDs, and error unwinding remain unavailable and require an explicit compatible
extension or ABI version bump before compiler code may emit them.

## Executable conformance

`compiler/abi/abi.h` and `compiler/abi/abi.c` are the machine-readable source
of this register subset. The GNU assembly renderer obtains argument register
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
and CFG-aware coalesced `phi` edge slots.

Run the host contract and generated-code checks with:

```sh
gmake compiler-abi-test miga68k-test compiler-execute-test compiler-spill-test
```
