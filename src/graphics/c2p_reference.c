#include "graphics/c2p_reference.h"

enum Magi80C2PStatus magi80_c2p_reference(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT],
    size_t plane_stride)
{
    size_t plane;
    size_t y;
    size_t byte_x;
    size_t bytes_per_row;

    if (chunky == NULL || planes == NULL) {
        return MAGI80_C2P_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        if (planes[plane] == NULL) {
            return MAGI80_C2P_INVALID_ARGUMENT;
        }
    }
    if (width == 0U || height == 0U || (width & 7U) != 0U) {
        return MAGI80_C2P_INVALID_DIMENSIONS;
    }

    bytes_per_row = width >> 3;
    if (chunky_stride < width || plane_stride < bytes_per_row) {
        return MAGI80_C2P_INVALID_STRIDE;
    }

    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);

        for (byte_x = 0U; byte_x < bytes_per_row; ++byte_x) {
            uint8_t packed[MAGI80_C2P_PLANE_COUNT] = {0U};
            size_t pixel;

            for (pixel = 0U; pixel < 8U; ++pixel) {
                uint8_t value = source_row[(byte_x << 3) + pixel];
                uint8_t output_bit = (uint8_t)(0x80U >> pixel);
                size_t logical_bit;

                for (logical_bit = 0U; logical_bit < 4U; ++logical_bit) {
                    if ((value & (uint8_t)(1U << (logical_bit + 4U))) !=
                        0U) {
                        packed[logical_bit << 1] |= output_bit;
                    }
                    if ((value & (uint8_t)(1U << logical_bit)) != 0U) {
                        packed[(logical_bit << 1) + 1U] |= output_bit;
                    }
                }
            }

            for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
                planes[plane][(y * plane_stride) + byte_x] = packed[plane];
            }
        }
    }
    return MAGI80_C2P_OK;
}
