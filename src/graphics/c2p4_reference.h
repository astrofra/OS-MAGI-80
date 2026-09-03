#ifndef MAGI80_GRAPHICS_C2P4_REFERENCE_H
#define MAGI80_GRAPHICS_C2P4_REFERENCE_H

#include <stddef.h>
#include <stdint.h>

enum {
    MAGI80_C2P4_PLANE_COUNT = 4,
    MAGI80_C2P4_PAIR_LUT_ENTRIES = 256
};

enum Magi80C2P4Status {
    MAGI80_C2P4_OK = 0,
    MAGI80_C2P4_INVALID_ARGUMENT,
    MAGI80_C2P4_INVALID_DIMENSIONS,
    MAGI80_C2P4_INVALID_STRIDE
};

/*
 * Convert one packed four-bit chunky layer to four separate bitplanes.
 * Even x is the high nibble and odd x is the low nibble.  Destination plane
 * 0 contains logical bit 0; plane 3 contains logical bit 3.  Bit 7 is the
 * leftmost pixel of every destination byte.
 */
enum Magi80C2P4Status magi80_c2p4_reference_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

/*
 * Convert one byte-per-pixel four-bit chunky layer.  Only the low nibble of
 * every source byte is significant.
 */
enum Magi80C2P4Status magi80_c2p4_reference_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

/*
 * Build the persistent 1 KiB pair-transpose table used by the lookup
 * candidates.  Table construction is setup work and is not part of a frame.
 */
void magi80_c2p4_build_pair_lut(
    uint32_t table[MAGI80_C2P4_PAIR_LUT_ENTRIES]);

enum Magi80C2P4Status magi80_c2p4_lookup_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MAGI80_C2P4_PAIR_LUT_ENTRIES]);

enum Magi80C2P4Status magi80_c2p4_lookup_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MAGI80_C2P4_PAIR_LUT_ENTRIES]);

/*
 * Table-free 32-pixel transpose candidates.  Width must be a non-zero
 * multiple of 32.  The portable versions define the bit-exact merge network;
 * the m68k entry points execute hand-written 68020 cores on target and fall
 * back to the same portable network on the host.
 */
enum Magi80C2P4Status magi80_c2p4_mask32_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

enum Magi80C2P4Status magi80_c2p4_mask32_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

enum Magi80C2P4Status magi80_c2p4_mask32_m68k_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

enum Magi80C2P4Status magi80_c2p4_mask32_m68k_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride);

/*
 * Hand-written 68020 pair-LUT candidates.  Non-m68k host builds use the C99
 * lookup implementation so the public contract remains natively testable;
 * target differential tests exercise the assembly cores.
 */
enum Magi80C2P4Status magi80_c2p4_pair_lut_m68k_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MAGI80_C2P4_PAIR_LUT_ENTRIES]);

enum Magi80C2P4Status magi80_c2p4_pair_lut_m68k_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MAGI80_C2P4_PAIR_LUT_ENTRIES]);

#endif
