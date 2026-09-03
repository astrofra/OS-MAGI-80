#ifndef MAGI80_TESTS_BENCHMARK_CHIPRAM_KERNELS_H
#define MAGI80_TESTS_BENCHMARK_CHIPRAM_KERNELS_H

#include <exec/types.h>

typedef ULONG (*Magi80ChipRamKernel)(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_current_stack_pointer(void);

ULONG magi80_chipram_write_byte(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_chipram_write_word(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_chipram_write_long(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_chipram_write_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_chipram_copy_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG magi80_chipram_read_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

#endif
