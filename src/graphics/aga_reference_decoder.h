#ifndef MAGI80_GRAPHICS_AGA_REFERENCE_DECODER_H
#define MAGI80_GRAPHICS_AGA_REFERENCE_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include "graphics/reference_compositor.h"

enum {
    MAGI80_AGA_REFERENCE_PLANE_COUNT = 8,
    MAGI80_AGA_REFERENCE_PLANE_ROW_BYTES =
        MAGI80_GRAPHICS_REFERENCE_WIDTH / 8
};

enum Magi80AgaReferenceStatus {
    MAGI80_AGA_REFERENCE_OK = 0,
    MAGI80_AGA_REFERENCE_INVALID_ARGUMENT,
    MAGI80_AGA_REFERENCE_INVALID_STRIDE
};

/*
 * Decode an eight-bitplane AGA dual-playfield frame into the canonical
 * 256x256 palette identities produced by the graphics reference compositor.
 *
 * Bitplanes 0/2/4/6 are PIXEL (PF1) bits 0/1/2/3.  Bitplanes 1/3/5/7 are
 * PLANAR (PF2) bits 0/1/2/3.  A non-zero PIXEL value is in front and maps to
 * canonical colors 16..30; PIXEL zero is transparent and reveals PLANAR
 * colors 0..15.  Bit 7 is the leftmost pixel in every source byte.
 *
 * The visible 256 bytes of every output row are overwritten on success;
 * output row padding is preserved.  All arguments are validated before the
 * output is modified.  Source planes and output storage must not overlap.
 */
enum Magi80AgaReferenceStatus magi80_aga_reference_decode_dual_playfield(
    const uint8_t *planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride,
    uint8_t *output,
    size_t output_stride);

#endif
