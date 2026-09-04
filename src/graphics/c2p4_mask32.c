#include "graphics/c2p4_reference.h"

static enum Miga80C2P4Status validate_mask32_conversion(
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
    if (width == 0U || height == 0U || (width & 31U) != 0U) {
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

static uint32_t load_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) |
           ((uint32_t)source[1] << 16) |
           ((uint32_t)source[2] << 8) |
           (uint32_t)source[3];
}

static void store_be32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value >> 24);
    destination[1] = (uint8_t)(value >> 16);
    destination[2] = (uint8_t)(value >> 8);
    destination[3] = (uint8_t)value;
}

static uint32_t pack_byte4_group(const uint8_t *source)
{
    uint32_t packed = 0U;
    size_t pixel;

    for (pixel = 0U; pixel < 8U; ++pixel) {
        packed = (packed << 4) | (uint32_t)(source[pixel] & 0x0fU);
    }
    return packed;
}

static void merge_bits(uint32_t *left, uint32_t *right,
                       unsigned int shift, uint32_t mask)
{
    uint32_t exchanged = (*left ^ (*right >> shift)) & mask;

    *left ^= exchanged;
    *right ^= exchanged << shift;
}

static void transpose32x4(uint32_t words[MIGA80_C2P4_PLANE_COUNT])
{
    /* Packed-nibble-specific five-pass transpose. */
    merge_bits(&words[0], &words[1], 8U, UINT32_C(0x00ff00ff));
    merge_bits(&words[2], &words[3], 8U, UINT32_C(0x00ff00ff));
    merge_bits(&words[0], &words[1], 2U, UINT32_C(0x33333333));
    merge_bits(&words[2], &words[3], 2U, UINT32_C(0x33333333));
    merge_bits(&words[0], &words[2], 16U, UINT32_C(0x0000ffff));
    merge_bits(&words[1], &words[3], 16U, UINT32_C(0x0000ffff));
    merge_bits(&words[0], &words[2], 4U, UINT32_C(0x0f0f0f0f));
    merge_bits(&words[1], &words[3], 4U, UINT32_C(0x0f0f0f0f));
    merge_bits(&words[0], &words[2], 1U, UINT32_C(0x55555555));
    merge_bits(&words[1], &words[3], 1U, UINT32_C(0x55555555));
}

static void store_transposed_block(
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t destination_offset,
    const uint32_t words[MIGA80_C2P4_PLANE_COUNT])
{
    /* The merge network ends in register order 3, 1, 2, 0. */
    store_be32(planes[0] + destination_offset, words[3]);
    store_be32(planes[1] + destination_offset, words[1]);
    store_be32(planes[2] + destination_offset, words[2]);
    store_be32(planes[3] + destination_offset, words[0]);
}

enum Miga80C2P4Status miga80_c2p4_mask32_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status = validate_mask32_conversion(
        chunky, width, height, chunky_stride, width >> 1, planes,
        plane_stride);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t block;

        for (block = 0U; block < (width >> 5); ++block) {
            const uint8_t *source = source_row + (block << 4);
            uint32_t words[MIGA80_C2P4_PLANE_COUNT];

            words[0] = load_be32(source);
            words[1] = load_be32(source + 4U);
            words[2] = load_be32(source + 8U);
            words[3] = load_be32(source + 12U);
            transpose32x4(words);
            store_transposed_block(planes, (y * plane_stride) +
                                             (block << 2), words);
        }
    }
    return MIGA80_C2P4_OK;
}

enum Miga80C2P4Status miga80_c2p4_mask32_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status = validate_mask32_conversion(
        chunky, width, height, chunky_stride, width, planes,
        plane_stride);
    size_t y;

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
    for (y = 0U; y < height; ++y) {
        const uint8_t *source_row = chunky + (y * chunky_stride);
        size_t block;

        for (block = 0U; block < (width >> 5); ++block) {
            const uint8_t *source = source_row + (block << 5);
            uint32_t words[MIGA80_C2P4_PLANE_COUNT];

            words[0] = pack_byte4_group(source);
            words[1] = pack_byte4_group(source + 8U);
            words[2] = pack_byte4_group(source + 16U);
            words[3] = pack_byte4_group(source + 24U);
            transpose32x4(words);
            store_transposed_block(planes, (y * plane_stride) +
                                             (block << 2), words);
        }
    }
    return MIGA80_C2P4_OK;
}
