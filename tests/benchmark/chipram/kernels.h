#ifndef MIGA80_TESTS_BENCHMARK_CHIPRAM_KERNELS_H
#define MIGA80_TESTS_BENCHMARK_CHIPRAM_KERNELS_H

#include <exec/types.h>

typedef ULONG (*Miga80ChipRamKernel)(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_current_stack_pointer(void);

ULONG miga80_chipram_write_byte(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_write_word(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_write_long(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_write_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_copy_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_read_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_read_byte(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_read_word(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_read_long(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_rmw_add_byte(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_rmw_add_word(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_rmw_add_long(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

ULONG miga80_chipram_rmw_add_long4(
    UBYTE *destination,
    const UBYTE *source,
    ULONG bytes,
    ULONG seed);

#endif
