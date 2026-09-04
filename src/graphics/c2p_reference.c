#include "graphics/c2p_reference.h"

#if defined(__GNUC__)
#define MIGA80_ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define MIGA80_ALWAYS_INLINE static
#endif

static enum Miga80C2PStatus validate_destination(
    size_t width,
    size_t height,
    uint8_t *planes[MIGA80_C2P_PLANE_COUNT],
    size_t plane_stride)
{
    size_t plane;

    if (planes == NULL) {
        return MIGA80_C2P_INVALID_ARGUMENT;
    }
    for (plane = 0U; plane < MIGA80_C2P_PLANE_COUNT; ++plane) {
        if (planes[plane] == NULL) {
            return MIGA80_C2P_INVALID_ARGUMENT;
        }
    }
    if (width == 0U || height == 0U || (width & 7U) != 0U) {
        return MIGA80_C2P_INVALID_DIMENSIONS;
    }
    if (plane_stride < (width >> 3)) {
        return MIGA80_C2P_INVALID_STRIDE;
    }
    return MIGA80_C2P_OK;
}

MIGA80_ALWAYS_INLINE void pack_pixel(
    uint8_t packed[MIGA80_C2P_PLANE_COUNT], uint8_t front, uint8_t back,
    size_t pixel)
{
    uint8_t output_bit = (uint8_t)(0x80U >> pixel);
    size_t logical_bit;

    for (logical_bit = 0U; logical_bit < 4U; ++logical_bit) {
        if ((front & (uint8_t)(1U << logical_bit)) != 0U) {
            packed[logical_bit << 1] |= output_bit;
        }
        if ((back & (uint8_t)(1U << logical_bit)) != 0U) {
            packed[(logical_bit << 1) + 1U] |= output_bit;
        }
    }
}

#undef MIGA80_ALWAYS_INLINE

enum Miga80C2PStatus miga80_c2p_reference(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2PStatus status;
    size_t y;
    size_t byte_x;
    size_t bytes_per_row;
    size_t plane;

    if (chunky == NULL) {
        return MIGA80_C2P_INVALID_ARGUMENT;
    }
    status = validate_destination(width, height, planes, plane_stride);
    if (status != MIGA80_C2P_OK) {
        return status;
    }
    if (chunky_stride < width) {
        return MIGA80_C2P_INVALID_STRIDE;
    }

    bytes_per_row = width >> 3;
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);

        for (byte_x = 0U; byte_x < bytes_per_row; ++byte_x) {
            uint8_t packed[MIGA80_C2P_PLANE_COUNT] = {0U};
            size_t pixel;

            for (pixel = 0U; pixel < 8U; ++pixel) {
                uint8_t value = source_row[(byte_x << 3) + pixel];

                pack_pixel(packed, (uint8_t)(value >> 4),
                           (uint8_t)(value & 0x0fU), pixel);
            }
            for (plane = 0U; plane < MIGA80_C2P_PLANE_COUNT; ++plane) {
                planes[plane][(y * plane_stride) + byte_x] = packed[plane];
            }
        }
    }
    return MIGA80_C2P_OK;
}
