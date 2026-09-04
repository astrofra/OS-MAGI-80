#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics/aga_reference_decoder.h"
#include "graphics/c2p_reference.h"
#include "graphics/reference_compositor.h"

#define FRAME_WIDTH MIGA80_GRAPHICS_REFERENCE_WIDTH
#define FRAME_HEIGHT MIGA80_GRAPHICS_REFERENCE_HEIGHT
#define FRAME_BYTES (FRAME_WIDTH * FRAME_HEIGHT)
#define PLANE_BYTES (FRAME_BYTES / 8U)
#define PADDED_PLANE_STRIDE 34U
#define PADDED_OUTPUT_STRIDE 260U

static uint8_t plane_storage[MIGA80_AGA_REFERENCE_PLANE_COUNT][PLANE_BYTES];
static uint8_t padded_plane_storage[MIGA80_AGA_REFERENCE_PLANE_COUNT]
                                   [PADDED_PLANE_STRIDE * FRAME_HEIGHT];
static uint8_t front[FRAME_BYTES];
static uint8_t back[FRAME_BYTES];
static uint8_t canonical_expected[FRAME_BYTES];
static uint8_t canonical_actual[FRAME_BYTES];
static uint8_t padded_output[PADDED_OUTPUT_STRIDE * FRAME_HEIGHT];

static void set_plane_pointers(
    const uint8_t *read_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT],
    uint8_t *write_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT],
    uint8_t storage[MIGA80_AGA_REFERENCE_PLANE_COUNT][PLANE_BYTES])
{
    size_t plane;

    for (plane = 0U; plane < MIGA80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        read_planes[plane] = storage[plane];
        write_planes[plane] = storage[plane];
    }
}

static int test_single_byte_golden(void)
{
    static const uint8_t source_byte[MIGA80_AGA_REFERENCE_PLANE_COUNT] = {
        0x8aU, 0x45U, 0x91U, 0x26U, 0xa2U, 0x15U, 0xc1U, 0x0eU
    };
    static const uint8_t expected[8] = {
        30U, 23U, 19U, 17U, 16U, 15U, 20U, 25U
    };
    const uint8_t *read_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    uint8_t *write_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    size_t plane;
    size_t pixel;

    memset(plane_storage, 0, sizeof(plane_storage));
    memset(canonical_actual, 0xcc, sizeof(canonical_actual));
    set_plane_pointers(read_planes, write_planes, plane_storage);
    for (plane = 0U; plane < MIGA80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        plane_storage[plane][0] = source_byte[plane];
    }
    if (miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, canonical_actual,
            FRAME_WIDTH) != MIGA80_AGA_REFERENCE_OK ||
        memcmp(canonical_actual, expected, sizeof(expected)) != 0) {
        return 0;
    }
    for (pixel = sizeof(expected); pixel < FRAME_BYTES; ++pixel) {
        if (canonical_actual[pixel] != 0U) {
            return 0;
        }
    }
    return 1;
}

static int test_compositor_c2p_decoder_differential(void)
{
    const uint8_t *read_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    uint8_t *write_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    struct Miga80GraphicsReferenceScene scene;
    size_t x;
    size_t y;

    for (y = 0U; y < FRAME_HEIGHT; ++y) {
        for (x = 0U; x < FRAME_WIDTH; ++x) {
            size_t offset = (y * FRAME_WIDTH) + x;
            uint8_t front_value =
                (uint8_t)(((x * 3U) + (y * 5U) + (x ^ y)) & 0x0fU);
            uint8_t back_value =
                (uint8_t)(((x >> 3) + (y >> 2) + 7U) & 0x0fU);

            if (((x + (y * 3U)) & 7U) < 3U) {
                front_value = 0U;
            }
            front[offset] = (uint8_t)(0xa0U | front_value);
            back[offset] = (uint8_t)(0xb0U | back_value);
        }
    }

    memset(&scene, 0, sizeof(scene));
    scene.planar.surface.pixels = back;
    scene.planar.surface.width = FRAME_WIDTH;
    scene.planar.surface.height = FRAME_HEIGHT;
    scene.planar.surface.stride = FRAME_WIDTH;
    scene.planar.width = FRAME_WIDTH;
    scene.planar.height = FRAME_HEIGHT;
    scene.planar.enabled = 1U;
    scene.pixel.surface.pixels = front;
    scene.pixel.surface.width = FRAME_WIDTH;
    scene.pixel.surface.height = FRAME_HEIGHT;
    scene.pixel.surface.stride = FRAME_WIDTH;
    scene.pixel.width = FRAME_WIDTH;
    scene.pixel.height = FRAME_HEIGHT;
    scene.pixel.enabled = 1U;

    memset(plane_storage, 0xcc, sizeof(plane_storage));
    set_plane_pointers(read_planes, write_planes, plane_storage);
    if (miga80_graphics_reference_compose(
            &scene, canonical_expected, FRAME_WIDTH) !=
            MIGA80_GRAPHICS_REFERENCE_OK ||
        miga80_c2p_reference_byte4_x2(
            front, back, FRAME_WIDTH, FRAME_HEIGHT, FRAME_WIDTH,
            FRAME_WIDTH, write_planes, FRAME_WIDTH / 8U) !=
            MIGA80_C2P_OK ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, canonical_actual,
            FRAME_WIDTH) != MIGA80_AGA_REFERENCE_OK) {
        return 0;
    }
    return memcmp(canonical_actual, canonical_expected, FRAME_BYTES) == 0;
}

static int test_strides_and_padding(void)
{
    const uint8_t *read_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    size_t plane;
    size_t x;
    size_t y;

    memset(padded_plane_storage, 0x5a, sizeof(padded_plane_storage));
    memset(padded_output, 0xcc, sizeof(padded_output));
    for (plane = 0U; plane < MIGA80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        read_planes[plane] = padded_plane_storage[plane];
        for (y = 0U; y < FRAME_HEIGHT; ++y) {
            memset(padded_plane_storage[plane] +
                       (y * PADDED_PLANE_STRIDE),
                   0, FRAME_WIDTH / 8U);
        }
    }
    padded_plane_storage[1][0] = 0x80U;
    padded_plane_storage[0][PADDED_PLANE_STRIDE] = 0x80U;

    if (miga80_aga_reference_decode_dual_playfield(
            read_planes, PADDED_PLANE_STRIDE, padded_output,
            PADDED_OUTPUT_STRIDE) != MIGA80_AGA_REFERENCE_OK) {
        return 0;
    }
    for (y = 0U; y < FRAME_HEIGHT; ++y) {
        for (x = 0U; x < FRAME_WIDTH; ++x) {
            uint8_t expected = 0U;

            if (x == 0U && y == 0U) {
                expected = 1U;
            } else if (x == 0U && y == 1U) {
                expected = 16U;
            }
            if (padded_output[(y * PADDED_OUTPUT_STRIDE) + x] !=
                expected) {
                return 0;
            }
        }
        for (x = FRAME_WIDTH; x < PADDED_OUTPUT_STRIDE; ++x) {
            if (padded_output[(y * PADDED_OUTPUT_STRIDE) + x] != 0xccU) {
                return 0;
            }
        }
        for (plane = 0U; plane < MIGA80_AGA_REFERENCE_PLANE_COUNT;
             ++plane) {
            if (padded_plane_storage[plane]
                                    [(y * PADDED_PLANE_STRIDE) + 32U] !=
                    0x5aU ||
                padded_plane_storage[plane]
                                    [(y * PADDED_PLANE_STRIDE) + 33U] !=
                    0x5aU) {
                return 0;
            }
        }
    }
    return 1;
}

static int test_argument_contract(void)
{
    const uint8_t *read_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    uint8_t *write_planes[MIGA80_AGA_REFERENCE_PLANE_COUNT];
    size_t byte_index;

    memset(plane_storage, 0, sizeof(plane_storage));
    memset(canonical_actual, 0x5a, sizeof(canonical_actual));
    set_plane_pointers(read_planes, write_planes, plane_storage);

    if (miga80_aga_reference_decode_dual_playfield(
            NULL, FRAME_WIDTH / 8U, canonical_actual, FRAME_WIDTH) !=
            MIGA80_AGA_REFERENCE_INVALID_ARGUMENT ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, NULL, FRAME_WIDTH) !=
            MIGA80_AGA_REFERENCE_INVALID_ARGUMENT ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, (FRAME_WIDTH / 8U) - 1U, canonical_actual,
            FRAME_WIDTH) != MIGA80_AGA_REFERENCE_INVALID_STRIDE ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, canonical_actual,
            FRAME_WIDTH - 1U) != MIGA80_AGA_REFERENCE_INVALID_STRIDE) {
        return 0;
    }

    read_planes[3] = NULL;
    if (miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, canonical_actual,
            FRAME_WIDTH) != MIGA80_AGA_REFERENCE_INVALID_ARGUMENT) {
        return 0;
    }
    read_planes[3] = plane_storage[3];
    if (miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, plane_storage[0],
            FRAME_WIDTH) != MIGA80_AGA_REFERENCE_INVALID_ARGUMENT ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, SIZE_MAX, canonical_actual, FRAME_WIDTH) !=
            MIGA80_AGA_REFERENCE_INVALID_STRIDE ||
        miga80_aga_reference_decode_dual_playfield(
            read_planes, FRAME_WIDTH / 8U, canonical_actual, SIZE_MAX) !=
            MIGA80_AGA_REFERENCE_INVALID_STRIDE) {
        return 0;
    }
    for (byte_index = 0U; byte_index < FRAME_BYTES; ++byte_index) {
        if (canonical_actual[byte_index] != 0x5aU) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    if (!test_single_byte_golden()) {
        puts("FAIL dual-playfield single-byte golden");
        return 1;
    }
    puts("PASS dual-playfield single-byte golden");

    if (!test_compositor_c2p_decoder_differential()) {
        puts("FAIL compositor to C2P to decoder differential");
        return 1;
    }
    puts("PASS compositor to C2P to decoder differential");

    if (!test_strides_and_padding()) {
        puts("FAIL source strides and output padding");
        return 1;
    }
    puts("PASS source strides and output padding");

    if (!test_argument_contract()) {
        puts("FAIL decoder argument contract");
        return 1;
    }
    puts("PASS decoder argument contract");
    puts("result=pass");
    return 0;
}
