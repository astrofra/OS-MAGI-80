#include "graphics/aga_reference_decoder.h"

#include <stdint.h>

static int storage_span(const void *storage, size_t stride,
                        size_t row_bytes, uintptr_t *begin, uintptr_t *end)
{
    size_t last_row_offset;
    size_t extent;

    if (stride > (SIZE_MAX - row_bytes) /
                     (MAGI80_GRAPHICS_REFERENCE_HEIGHT - 1U)) {
        return 0;
    }
    last_row_offset =
        stride * (MAGI80_GRAPHICS_REFERENCE_HEIGHT - 1U);
    extent = last_row_offset + row_bytes;
    *begin = (uintptr_t)storage;
    if (*begin > UINTPTR_MAX - extent) {
        return 0;
    }
    *end = *begin + extent;
    return 1;
}

static int spans_overlap(uintptr_t left_begin, uintptr_t left_end,
                         uintptr_t right_begin, uintptr_t right_end)
{
    return left_begin < right_end && right_begin < left_end;
}

static enum Magi80AgaReferenceStatus validate_arguments(
    const uint8_t *planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride,
    uint8_t *output,
    size_t output_stride)
{
    uintptr_t output_begin;
    uintptr_t output_end;
    size_t plane;

    if (planes == NULL || output == NULL) {
        return MAGI80_AGA_REFERENCE_INVALID_ARGUMENT;
    }
    if (plane_stride < MAGI80_AGA_REFERENCE_PLANE_ROW_BYTES ||
        output_stride < MAGI80_GRAPHICS_REFERENCE_WIDTH) {
        return MAGI80_AGA_REFERENCE_INVALID_STRIDE;
    }
    if (!storage_span(output, output_stride,
                      MAGI80_GRAPHICS_REFERENCE_WIDTH, &output_begin,
                      &output_end)) {
        return MAGI80_AGA_REFERENCE_INVALID_STRIDE;
    }
    for (plane = 0U; plane < MAGI80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        uintptr_t plane_begin;
        uintptr_t plane_end;

        if (planes[plane] == NULL) {
            return MAGI80_AGA_REFERENCE_INVALID_ARGUMENT;
        }
        if (!storage_span(planes[plane], plane_stride,
                          MAGI80_AGA_REFERENCE_PLANE_ROW_BYTES,
                          &plane_begin, &plane_end)) {
            return MAGI80_AGA_REFERENCE_INVALID_STRIDE;
        }
        if (spans_overlap(plane_begin, plane_end, output_begin,
                          output_end)) {
            return MAGI80_AGA_REFERENCE_INVALID_ARGUMENT;
        }
    }
    return MAGI80_AGA_REFERENCE_OK;
}

enum Magi80AgaReferenceStatus magi80_aga_reference_decode_dual_playfield(
    const uint8_t *planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride,
    uint8_t *output,
    size_t output_stride)
{
    enum Magi80AgaReferenceStatus status =
        validate_arguments(planes, plane_stride, output, output_stride);
    size_t y;

    if (status != MAGI80_AGA_REFERENCE_OK) {
        return status;
    }
    for (y = 0U; y < MAGI80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        size_t byte_x;

        for (byte_x = 0U;
             byte_x < MAGI80_AGA_REFERENCE_PLANE_ROW_BYTES; ++byte_x) {
            size_t pixel;

            for (pixel = 0U; pixel < 8U; ++pixel) {
                uint8_t mask = (uint8_t)(0x80U >> pixel);
                uint8_t front = 0U;
                uint8_t back = 0U;
                size_t logical_bit;

                for (logical_bit = 0U; logical_bit < 4U;
                     ++logical_bit) {
                    size_t front_plane = logical_bit << 1;
                    size_t back_plane = front_plane + 1U;
                    size_t source_offset = (y * plane_stride) + byte_x;

                    if ((planes[front_plane][source_offset] & mask) != 0U) {
                        front |= (uint8_t)(1U << logical_bit);
                    }
                    if ((planes[back_plane][source_offset] & mask) != 0U) {
                        back |= (uint8_t)(1U << logical_bit);
                    }
                }
                output[(y * output_stride) + (byte_x << 3) + pixel] =
                    front != 0U
                        ? (uint8_t)(MAGI80_GRAPHICS_OVERLAY_COLOR_BASE +
                                    front)
                        : back;
            }
        }
    }
    return MAGI80_AGA_REFERENCE_OK;
}
