#ifndef MAGI80_GRAPHICS_C2P_REFERENCE_H
#define MAGI80_GRAPHICS_C2P_REFERENCE_H

#include <stddef.h>
#include <stdint.h>

enum {
    MAGI80_C2P_PLANE_COUNT = 8
};

enum Magi80C2PStatus {
    MAGI80_C2P_OK = 0,
    MAGI80_C2P_INVALID_ARGUMENT,
    MAGI80_C2P_INVALID_DIMENSIONS,
    MAGI80_C2P_INVALID_STRIDE
};

/*
 * Convert combined MAGI-80 pixels to an Amiga dual-playfield bitmap.
 *
 * Each chunky byte is 0xFB: F is the four-bit FRONT pixel and B is the
 * four-bit BACK pixel.  The destination order is the hardware bitplane
 * order used by AGA dual-playfield mode:
 *
 *   plane 0/2/4/6 = FRONT bits 0/1/2/3 (BPL1/BPL3/BPL5/BPL7)
 *   plane 1/3/5/7 = BACK  bits 0/1/2/3 (BPL2/BPL4/BPL6/BPL8)
 *
 * Width must be a non-zero multiple of eight.  Height must be non-zero.
 * Source and destination storage must not overlap.  Every destination
 * plane must provide height rows of at least plane_stride bytes; the source
 * must provide height rows of at least chunky_stride bytes.
 */
enum Magi80C2PStatus magi80_c2p_reference(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT],
    size_t plane_stride);

#endif
