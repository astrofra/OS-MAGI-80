#include "graphics/c2p4_reference.h"

static enum Miga80C2P4Status validate_lookup_conversion(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    size_t required_chunky_bytes,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    size_t plane;

    if (chunky == NULL || planes == NULL || table == NULL) {
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

static uint32_t spread_nibble(uint8_t color)
{
    uint32_t spread = 0U;

    if ((color & 0x01U) != 0U) {
        spread |= UINT32_C(0x80000000);
    }
    if ((color & 0x02U) != 0U) {
        spread |= UINT32_C(0x00800000);
    }
    if ((color & 0x04U) != 0U) {
        spread |= UINT32_C(0x00008000);
    }
    if ((color & 0x08U) != 0U) {
        spread |= UINT32_C(0x00000080);
    }
    return spread;
}

void miga80_c2p4_build_pair_lut(
    uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    size_t pair;

    if (table == NULL) {
        return;
    }
    for (pair = 0U; pair < MIGA80_C2P4_PAIR_LUT_ENTRIES; ++pair) {
        uint8_t high = (uint8_t)(pair >> 4);
        uint8_t low = (uint8_t)(pair & 0x0fU);

        table[pair] = spread_nibble(high) | (spread_nibble(low) >> 1);
    }
}

static void store_transpose(uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
                            size_t destination_offset, uint32_t transpose)
{
    planes[0][destination_offset] = (uint8_t)(transpose >> 24);
    planes[1][destination_offset] = (uint8_t)(transpose >> 16);
    planes[2][destination_offset] = (uint8_t)(transpose >> 8);
    planes[3][destination_offset] = (uint8_t)transpose;
}

enum Miga80C2P4Status miga80_c2p4_lookup_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    enum Miga80C2P4Status status = validate_lookup_conversion(
        chunky, width, height, chunky_stride, width >> 1, planes,
        plane_stride, table);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t byte_x;

        for (byte_x = 0U; byte_x < (width >> 3); ++byte_x) {
            const uint8_t *source = source_row + (byte_x << 2);
            uint32_t transpose = table[source[0]] |
                                 (table[source[1]] >> 2) |
                                 (table[source[2]] >> 4) |
                                 (table[source[3]] >> 6);

            store_transpose(planes, (y * plane_stride) + byte_x,
                            transpose);
        }
    }
    return MIGA80_C2P4_OK;
}

enum Miga80C2P4Status miga80_c2p4_lookup_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    enum Miga80C2P4Status status = validate_lookup_conversion(
        chunky, width, height, chunky_stride, width, planes, plane_stride,
        table);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t byte_x;

        for (byte_x = 0U; byte_x < (width >> 3); ++byte_x) {
            const uint8_t *source = source_row + (byte_x << 3);
            uint8_t pair0 =
                (uint8_t)(((source[0] & 0x0fU) << 4) |
                          (source[1] & 0x0fU));
            uint8_t pair1 =
                (uint8_t)(((source[2] & 0x0fU) << 4) |
                          (source[3] & 0x0fU));
            uint8_t pair2 =
                (uint8_t)(((source[4] & 0x0fU) << 4) |
                          (source[5] & 0x0fU));
            uint8_t pair3 =
                (uint8_t)(((source[6] & 0x0fU) << 4) |
                          (source[7] & 0x0fU));
            uint32_t transpose = table[pair0] | (table[pair1] >> 2) |
                                 (table[pair2] >> 4) |
                                 (table[pair3] >> 6);

            store_transpose(planes, (y * plane_stride) + byte_x,
                            transpose);
        }
    }
    return MIGA80_C2P4_OK;
}
