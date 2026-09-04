#include "graphics/c2p4_reference.h"

static enum Miga80C2P4Status validate_conversion(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    size_t required_chunky_bytes,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    size_t plane;

    if (chunky == NULL || planes == NULL) {
        return MIGA80_C2P4_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < MIGA80_C2P4_PLANE_COUNT; ++plane) {
        if (planes[plane] == NULL) {
            return MIGA80_C2P4_INVALID_ARGUMENT;
        }
    }
    if (width == 0U || height == 0U || (width & 7U) != 0U) {
        return MIGA80_C2P4_INVALID_DIMENSIONS;
    }
    if (chunky_stride < required_chunky_bytes ||
        plane_stride < (width >> 3) ||
        height > SIZE_MAX / chunky_stride ||
        height > SIZE_MAX / plane_stride) {
        return MIGA80_C2P4_INVALID_STRIDE;
    }
    return MIGA80_C2P4_OK;
}

static void pack_pixel(uint8_t packed[MIGA80_C2P4_PLANE_COUNT],
                       uint8_t color, size_t pixel)
{
    uint8_t output_bit = (uint8_t)(0x80U >> pixel);
    size_t logical_bit;

    for (logical_bit = 0U; logical_bit < MIGA80_C2P4_PLANE_COUNT;
         ++logical_bit) {
        if ((color & (uint8_t)(1U << logical_bit)) != 0U) {
            packed[logical_bit] |= output_bit;
        }
    }
}

static void store_group(uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
                        size_t plane_stride, size_t y, size_t byte_x,
                        const uint8_t packed[MIGA80_C2P4_PLANE_COUNT])
{
    size_t plane;

    for (plane = 0U; plane < MIGA80_C2P4_PLANE_COUNT; ++plane) {
        planes[plane][(y * plane_stride) + byte_x] = packed[plane];
    }
}

enum Miga80C2P4Status miga80_c2p4_reference_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status =
        validate_conversion(chunky, width, height, chunky_stride,
                            width >> 1, planes, plane_stride);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t byte_x;

        for (byte_x = 0U; byte_x < (width >> 3); ++byte_x) {
            uint8_t packed[MIGA80_C2P4_PLANE_COUNT] = {0U};
            size_t pixel;

            for (pixel = 0U; pixel < 8U; ++pixel) {
                size_t x = (byte_x << 3) + pixel;
                uint8_t source_pair = source_row[x >> 1];
                unsigned int shift = (x & 1U) == 0U ? 4U : 0U;

                pack_pixel(packed,
                           (uint8_t)((source_pair >> shift) & 0x0fU),
                           pixel);
            }
            store_group(planes, plane_stride, y, byte_x, packed);
        }
    }
    return MIGA80_C2P4_OK;
}

enum Miga80C2P4Status miga80_c2p4_reference_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status =
        validate_conversion(chunky, width, height, chunky_stride, width,
                            planes, plane_stride);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t byte_x;

        for (byte_x = 0U; byte_x < (width >> 3); ++byte_x) {
            uint8_t packed[MIGA80_C2P4_PLANE_COUNT] = {0U};
            size_t pixel;

            for (pixel = 0U; pixel < 8U; ++pixel) {
                pack_pixel(packed,
                           (uint8_t)(source_row[(byte_x << 3) + pixel] &
                                     0x0fU),
                           pixel);
            }
            store_group(planes, plane_stride, y, byte_x, packed);
        }
    }
    return MIGA80_C2P4_OK;
}
