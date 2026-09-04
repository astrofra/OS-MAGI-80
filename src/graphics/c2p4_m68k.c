#include "graphics/c2p4_reference.h"

static enum Miga80C2P4Status validate_m68k_conversion(
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

static enum Miga80C2P4Status validate_mask32_m68k_conversion(
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

#if defined(__m68k__)
extern void miga80_c2p4_pair_lut_m68k_packed4_core(
    const uint8_t *chunky,
    size_t groups_per_row,
    size_t height,
    size_t source_row_skip,
    uint8_t *plane0,
    uint8_t *plane1,
    uint8_t *plane2,
    uint8_t *plane3,
    size_t destination_row_skip,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES]);

extern void miga80_c2p4_pair_lut_m68k_byte4_core(
    const uint8_t *chunky,
    size_t groups_per_row,
    size_t height,
    size_t source_row_skip,
    uint8_t *plane0,
    uint8_t *plane1,
    uint8_t *plane2,
    uint8_t *plane3,
    size_t destination_row_skip,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES]);

extern void miga80_c2p4_mask32_m68k_packed4_core(
    const uint8_t *chunky,
    size_t blocks_per_row,
    size_t height,
    size_t source_row_skip,
    uint8_t *plane0,
    uint8_t *plane1,
    uint8_t *plane2,
    uint8_t *plane3,
    size_t destination_row_skip);

extern void miga80_c2p4_mask32_m68k_byte4_core(
    const uint8_t *chunky,
    size_t blocks_per_row,
    size_t height,
    size_t source_row_skip,
    uint8_t *plane0,
    uint8_t *plane1,
    uint8_t *plane2,
    uint8_t *plane3,
    size_t destination_row_skip);
#endif

enum Miga80C2P4Status miga80_c2p4_mask32_m68k_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status = validate_mask32_m68k_conversion(
        chunky, width, height, chunky_stride, width >> 1, planes,
        plane_stride);

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
#if defined(__m68k__)
    miga80_c2p4_mask32_m68k_packed4_core(
        chunky, width >> 5, height, chunky_stride - (width >> 1),
        planes[0], planes[1], planes[2], planes[3],
        plane_stride - (width >> 3));
    return MIGA80_C2P4_OK;
#else
    return miga80_c2p4_mask32_packed4(
        chunky, width, height, chunky_stride, planes, plane_stride);
#endif
}

enum Miga80C2P4Status miga80_c2p4_mask32_m68k_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    enum Miga80C2P4Status status = validate_mask32_m68k_conversion(
        chunky, width, height, chunky_stride, width, planes,
        plane_stride);

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
#if defined(__m68k__)
    miga80_c2p4_mask32_m68k_byte4_core(
        chunky, width >> 5, height, chunky_stride - width, planes[0],
        planes[1], planes[2], planes[3],
        plane_stride - (width >> 3));
    return MIGA80_C2P4_OK;
#else
    return miga80_c2p4_mask32_byte4(
        chunky, width, height, chunky_stride, planes, plane_stride);
#endif
}

enum Miga80C2P4Status miga80_c2p4_pair_lut_m68k_packed4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    enum Miga80C2P4Status status = validate_m68k_conversion(
        chunky, width, height, chunky_stride, width >> 1, planes,
        plane_stride, table);

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
#if defined(__m68k__)
    miga80_c2p4_pair_lut_m68k_packed4_core(
        chunky, width >> 3, height, chunky_stride - (width >> 1),
        planes[0], planes[1], planes[2], planes[3],
        plane_stride - (width >> 3), table);
    return MIGA80_C2P4_OK;
#else
    return miga80_c2p4_lookup_packed4(
        chunky, width, height, chunky_stride, planes, plane_stride, table);
#endif
}

enum Miga80C2P4Status miga80_c2p4_pair_lut_m68k_byte4(
    const uint8_t *chunky,
    size_t width,
    size_t height,
    size_t chunky_stride,
    uint8_t *planes[MIGA80_C2P4_PLANE_COUNT],
    size_t plane_stride,
    const uint32_t table[MIGA80_C2P4_PAIR_LUT_ENTRIES])
{
    enum Miga80C2P4Status status = validate_m68k_conversion(
        chunky, width, height, chunky_stride, width, planes, plane_stride,
        table);

    if (status != MIGA80_C2P4_OK) {
        return status;
    }
#if defined(__m68k__)
    miga80_c2p4_pair_lut_m68k_byte4_core(
        chunky, width >> 3, height, chunky_stride - width, planes[0],
        planes[1], planes[2], planes[3],
        plane_stride - (width >> 3), table);
    return MIGA80_C2P4_OK;
#else
    return miga80_c2p4_lookup_byte4(
        chunky, width, height, chunky_stride, planes, plane_stride, table);
#endif
}
