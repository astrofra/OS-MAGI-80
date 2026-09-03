#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics/aga_reference_decoder.h"
#include "graphics/c2p4_reference.h"
#include "graphics/reference_compositor.h"

#define MAX_WIDTH MAGI80_GRAPHICS_REFERENCE_WIDTH
#define MAX_HEIGHT MAGI80_GRAPHICS_REFERENCE_HEIGHT
#define MAX_PIXELS (MAX_WIDTH * MAX_HEIGHT)
#define MAX_PACKED_BYTES (MAX_PIXELS / 2U)
#define MAX_PLANE_BYTES (MAX_PIXELS / 8U)

static uint8_t front_byte[MAX_PIXELS];
static uint8_t front_packed[MAX_PACKED_BYTES];
static uint8_t planar_byte[MAX_PIXELS];
static uint8_t reference_planes[MAGI80_C2P4_PLANE_COUNT]
                               [MAX_PLANE_BYTES];
static uint8_t candidate_planes[MAGI80_C2P4_PLANE_COUNT]
                               [MAX_PLANE_BYTES];
static uint8_t physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT]
                              [MAX_PLANE_BYTES];
static uint8_t canonical_expected[MAX_PIXELS];
static uint8_t canonical_actual[MAX_PIXELS];
static uint32_t pair_lut[MAGI80_C2P4_PAIR_LUT_ENTRIES];

static void set_four_planes(
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    uint8_t storage[MAGI80_C2P4_PLANE_COUNT][MAX_PLANE_BYTES])
{
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        planes[plane] = storage[plane];
    }
}

static int planes_match(
    const uint8_t left[MAGI80_C2P4_PLANE_COUNT][MAX_PLANE_BYTES],
    const uint8_t right[MAGI80_C2P4_PLANE_COUNT][MAX_PLANE_BYTES],
    size_t plane_bytes)
{
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        if (memcmp(left[plane], right[plane], plane_bytes) != 0) {
            return 0;
        }
    }
    return 1;
}

static int single_output_matches(
    const uint8_t storage[MAGI80_C2P4_PLANE_COUNT],
    const uint8_t expected[MAGI80_C2P4_PLANE_COUNT])
{
    return memcmp(storage, expected, MAGI80_C2P4_PLANE_COUNT) == 0;
}

static int test_single_byte_golden(void)
{
    static const uint8_t packed[4] = {0xf8U, 0x42U, 0x10U, 0x5aU};
    static const uint8_t byte[8] = {
        0xcfU, 0xc8U, 0xc4U, 0xc2U, 0xc1U, 0xc0U, 0xc5U, 0xcaU
    };
    static const uint8_t expected[MAGI80_C2P4_PLANE_COUNT] = {
        0x8aU, 0x91U, 0xa2U, 0xc1U
    };
    uint8_t storage[MAGI80_C2P4_PLANE_COUNT] = {0U};
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT];
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        planes[plane] = &storage[plane];
    }
    if (magi80_c2p4_reference_packed4(packed, 8U, 1U, 4U, planes,
                                      1U) != MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    memset(storage, 0, sizeof(storage));
    if (magi80_c2p4_reference_byte4(byte, 8U, 1U, 8U, planes, 1U) !=
            MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    memset(storage, 0, sizeof(storage));
    if (magi80_c2p4_lookup_packed4(packed, 8U, 1U, 4U, planes, 1U,
                                   pair_lut) != MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    memset(storage, 0, sizeof(storage));
    if (magi80_c2p4_lookup_byte4(byte, 8U, 1U, 8U, planes, 1U,
                                pair_lut) != MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    memset(storage, 0, sizeof(storage));
    if (magi80_c2p4_pair_lut_m68k_packed4(
            packed, 8U, 1U, 4U, planes, 1U, pair_lut) !=
            MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    memset(storage, 0, sizeof(storage));
    if (magi80_c2p4_pair_lut_m68k_byte4(
            byte, 8U, 1U, 8U, planes, 1U, pair_lut) !=
            MAGI80_C2P4_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    return 1;
}

static void prepare_profile(size_t width, size_t height)
{
    size_t x;
    size_t y;

    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            size_t offset = (y * width) + x;
            uint8_t color =
                (uint8_t)(((x * 3U) + (y * 5U) + (x ^ y)) & 0x0fU);

            if (((x + (y * 3U)) & 7U) < 3U) {
                color = 0U;
            }
            front_byte[offset] = (uint8_t)(0xa0U | color);
            if ((x & 1U) == 0U) {
                front_packed[offset >> 1] = (uint8_t)(color << 4);
            } else {
                front_packed[offset >> 1] |= color;
            }
        }
    }
}

static int test_profile_equivalence(void)
{
    static const size_t widths[3] = {160U, 192U, 256U};
    static const size_t heights[3] = {128U, 160U, 256U};
    uint8_t *reference[MAGI80_C2P4_PLANE_COUNT];
    uint8_t *candidate[MAGI80_C2P4_PLANE_COUNT];
    size_t profile;

    set_four_planes(reference, reference_planes);
    set_four_planes(candidate, candidate_planes);
    for (profile = 0U; profile < 3U; ++profile) {
        size_t width = widths[profile];
        size_t height = heights[profile];
        size_t plane_stride = width >> 3;
        size_t plane_bytes = plane_stride * height;

        prepare_profile(width, height);
        memset(reference_planes, 0xcc, sizeof(reference_planes));
        if (magi80_c2p4_reference_packed4(
                front_packed, width, height, width >> 1, reference,
                plane_stride) != MAGI80_C2P4_OK) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_reference_byte4(
                front_byte, width, height, width, candidate,
                plane_stride) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_lookup_packed4(
                front_packed, width, height, width >> 1, candidate,
                plane_stride, pair_lut) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_lookup_byte4(
                front_byte, width, height, width, candidate, plane_stride,
                pair_lut) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_pair_lut_m68k_packed4(
                front_packed, width, height, width >> 1, candidate,
                plane_stride, pair_lut) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_pair_lut_m68k_byte4(
                front_byte, width, height, width, candidate, plane_stride,
                pair_lut) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_mask32_packed4(
                front_packed, width, height, width >> 1, candidate,
                plane_stride) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_mask32_byte4(
                front_byte, width, height, width, candidate,
                plane_stride) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_mask32_m68k_packed4(
                front_packed, width, height, width >> 1, candidate,
                plane_stride) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }

        memset(candidate_planes, 0xdd, sizeof(candidate_planes));
        if (magi80_c2p4_mask32_m68k_byte4(
                front_byte, width, height, width, candidate,
                plane_stride) != MAGI80_C2P4_OK ||
            !planes_match(reference_planes, candidate_planes,
                          plane_bytes)) {
            return 0;
        }
    }
    return 1;
}

static int strided_output_matches(
    const uint8_t storage[MAGI80_C2P4_PLANE_COUNT][6])
{
    static const uint8_t expected[MAGI80_C2P4_PLANE_COUNT][6] = {
        {0xffU, 0x00U, 0x5aU, 0xaaU, 0xffU, 0x5aU},
        {0x00U, 0xffU, 0x5aU, 0xaaU, 0xffU, 0x5aU},
        {0x00U, 0x00U, 0x5aU, 0xaaU, 0xffU, 0x5aU},
        {0x00U, 0x00U, 0x5aU, 0xaaU, 0xffU, 0x5aU}
    };

    return memcmp(storage, expected, sizeof(expected)) == 0;
}

static int test_strides_and_arguments(void)
{
    enum {
        WIDTH = 16,
        HEIGHT = 2,
        PACKED_STRIDE = 9,
        BYTE_STRIDE = 18,
        PLANE_STRIDE = 3
    };
    uint8_t packed[HEIGHT * PACKED_STRIDE];
    uint8_t byte[HEIGHT * BYTE_STRIDE];
    uint8_t storage[MAGI80_C2P4_PLANE_COUNT][HEIGHT * PLANE_STRIDE];
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT];
    size_t plane;
    size_t x;
    size_t y;

    memset(packed, 0xcc, sizeof(packed));
    memset(byte, 0xcc, sizeof(byte));
    for (y = 0U; y < HEIGHT; ++y) {
        memset(packed + (y * PACKED_STRIDE), 0, WIDTH / 2U);
        for (x = 0U; x < WIDTH; ++x) {
            uint8_t color;
            size_t pair_offset = (y * PACKED_STRIDE) + (x >> 1);

            if (y == 0U) {
                color = x < 8U ? 1U : 2U;
            } else {
                color = x < 8U && (x & 1U) != 0U ? 0U : 15U;
            }
            byte[(y * BYTE_STRIDE) + x] = (uint8_t)(0xd0U | color);
            if ((x & 1U) == 0U) {
                packed[pair_offset] = (uint8_t)(color << 4);
            } else {
                packed[pair_offset] |= color;
            }
        }
    }
    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        planes[plane] = storage[plane];
    }

    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_reference_packed4(
            packed, WIDTH, HEIGHT, PACKED_STRIDE, planes,
            PLANE_STRIDE) != MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_reference_byte4(byte, WIDTH, HEIGHT, BYTE_STRIDE,
                                    planes, PLANE_STRIDE) !=
            MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_lookup_packed4(
            packed, WIDTH, HEIGHT, PACKED_STRIDE, planes, PLANE_STRIDE,
            pair_lut) != MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_lookup_byte4(byte, WIDTH, HEIGHT, BYTE_STRIDE, planes,
                                 PLANE_STRIDE, pair_lut) !=
            MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_pair_lut_m68k_packed4(
            packed, WIDTH, HEIGHT, PACKED_STRIDE, planes, PLANE_STRIDE,
            pair_lut) != MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_pair_lut_m68k_byte4(
            byte, WIDTH, HEIGHT, BYTE_STRIDE, planes, PLANE_STRIDE,
            pair_lut) != MAGI80_C2P4_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }

    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p4_reference_byte4(NULL, 8U, 1U, 8U, planes, 1U) !=
            MAGI80_C2P4_INVALID_ARGUMENT ||
        magi80_c2p4_reference_byte4(byte, 0U, 1U, 8U, planes, 1U) !=
            MAGI80_C2P4_INVALID_DIMENSIONS ||
        magi80_c2p4_reference_byte4(byte, 9U, 1U, 9U, planes, 2U) !=
            MAGI80_C2P4_INVALID_DIMENSIONS ||
        magi80_c2p4_reference_byte4(byte, 16U, 2U, 15U, planes, 2U) !=
            MAGI80_C2P4_INVALID_STRIDE ||
        magi80_c2p4_reference_packed4(
            packed, 16U, 2U, 7U, planes, 2U) !=
            MAGI80_C2P4_INVALID_STRIDE ||
        magi80_c2p4_lookup_byte4(byte, 16U, 2U, 16U, planes, 2U,
                                 NULL) !=
            MAGI80_C2P4_INVALID_ARGUMENT ||
        magi80_c2p4_mask32_byte4(byte, 16U, 2U, 16U, planes, 2U) !=
            MAGI80_C2P4_INVALID_DIMENSIONS ||
        magi80_c2p4_mask32_m68k_packed4(
            packed, 16U, 2U, 8U, planes, 2U) !=
            MAGI80_C2P4_INVALID_DIMENSIONS) {
        return 0;
    }
    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        for (x = 0U; x < sizeof(storage[plane]); ++x) {
            if (storage[plane][x] != 0x5aU) {
                return 0;
            }
        }
    }
    return 1;
}

static void prepare_planar(void)
{
    size_t x;
    size_t y;

    for (y = 0U; y < MAX_HEIGHT; ++y) {
        for (x = 0U; x < MAX_WIDTH; ++x) {
            planar_byte[(y * MAX_WIDTH) + x] =
                (uint8_t)(0xb0U |
                          (((x >> 3) + (y >> 2) + 7U) & 0x0fU));
        }
    }
}

static int run_canonical_backend(size_t width, size_t height,
                                 size_t origin_x, size_t origin_y,
                                 unsigned int backend)
{
    uint8_t *front_planes[MAGI80_C2P4_PLANE_COUNT];
    uint8_t *back_planes[MAGI80_C2P4_PLANE_COUNT];
    const uint8_t *read_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT];
    size_t plane;
    enum Magi80C2P4Status status;

    memset(physical_planes, 0, sizeof(physical_planes));
    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        front_planes[plane] =
            physical_planes[plane << 1] +
            (origin_y * (MAX_WIDTH / 8U)) + (origin_x / 8U);
        back_planes[plane] = physical_planes[(plane << 1) + 1U];
    }
    for (plane = 0U; plane < MAGI80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        read_planes[plane] = physical_planes[plane];
    }
    if (magi80_c2p4_reference_byte4(
            planar_byte, MAX_WIDTH, MAX_HEIGHT, MAX_WIDTH, back_planes,
            MAX_WIDTH / 8U) != MAGI80_C2P4_OK) {
        return 0;
    }

    if (backend == 0U) {
        status = magi80_c2p4_reference_packed4(
            front_packed, width, height, width >> 1, front_planes,
            MAX_WIDTH / 8U);
    } else if (backend == 1U) {
        status = magi80_c2p4_reference_byte4(
            front_byte, width, height, width, front_planes,
            MAX_WIDTH / 8U);
    } else if (backend == 2U) {
        status = magi80_c2p4_lookup_packed4(
            front_packed, width, height, width >> 1, front_planes,
            MAX_WIDTH / 8U, pair_lut);
    } else if (backend == 3U) {
        status = magi80_c2p4_lookup_byte4(
            front_byte, width, height, width, front_planes,
            MAX_WIDTH / 8U, pair_lut);
    } else if (backend == 4U) {
        status = magi80_c2p4_pair_lut_m68k_packed4(
            front_packed, width, height, width >> 1, front_planes,
            MAX_WIDTH / 8U, pair_lut);
    } else if (backend == 5U) {
        status = magi80_c2p4_pair_lut_m68k_byte4(
            front_byte, width, height, width, front_planes,
            MAX_WIDTH / 8U, pair_lut);
    } else if (backend == 6U) {
        status = magi80_c2p4_mask32_packed4(
            front_packed, width, height, width >> 1, front_planes,
            MAX_WIDTH / 8U);
    } else if (backend == 7U) {
        status = magi80_c2p4_mask32_byte4(
            front_byte, width, height, width, front_planes,
            MAX_WIDTH / 8U);
    } else if (backend == 8U) {
        status = magi80_c2p4_mask32_m68k_packed4(
            front_packed, width, height, width >> 1, front_planes,
            MAX_WIDTH / 8U);
    } else {
        status = magi80_c2p4_mask32_m68k_byte4(
            front_byte, width, height, width, front_planes,
            MAX_WIDTH / 8U);
    }
    if (status != MAGI80_C2P4_OK ||
        magi80_aga_reference_decode_dual_playfield(
            read_planes, MAX_WIDTH / 8U, canonical_actual,
            MAX_WIDTH) != MAGI80_AGA_REFERENCE_OK) {
        return 0;
    }
    return memcmp(canonical_actual, canonical_expected, MAX_PIXELS) == 0;
}

static int test_canonical_differentials(void)
{
    static const size_t widths[3] = {160U, 192U, 256U};
    static const size_t heights[3] = {128U, 160U, 256U};
    struct Magi80GraphicsReferenceScene scene;
    size_t profile;

    prepare_planar();
    for (profile = 0U; profile < 3U; ++profile) {
        size_t width = widths[profile];
        size_t height = heights[profile];
        size_t origin_x = (MAX_WIDTH - width) / 2U;
        size_t origin_y = (MAX_HEIGHT - height) / 2U;
        unsigned int backend;

        prepare_profile(width, height);
        memset(&scene, 0, sizeof(scene));
        scene.planar.surface.pixels = planar_byte;
        scene.planar.surface.width = MAX_WIDTH;
        scene.planar.surface.height = MAX_HEIGHT;
        scene.planar.surface.stride = MAX_WIDTH;
        scene.planar.width = MAX_WIDTH;
        scene.planar.height = MAX_HEIGHT;
        scene.planar.enabled = 1U;
        scene.pixel.surface.pixels = front_byte;
        scene.pixel.surface.width = width;
        scene.pixel.surface.height = height;
        scene.pixel.surface.stride = width;
        scene.pixel.width = width;
        scene.pixel.height = height;
        scene.pixel.screen_x = (int32_t)origin_x;
        scene.pixel.screen_y = (int32_t)origin_y;
        scene.pixel.enabled = 1U;
        if (magi80_graphics_reference_compose(
                &scene, canonical_expected, MAX_WIDTH) !=
            MAGI80_GRAPHICS_REFERENCE_OK) {
            return 0;
        }
        for (backend = 0U; backend < 10U; ++backend) {
            if (!run_canonical_backend(width, height, origin_x, origin_y,
                                       backend)) {
                return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    magi80_c2p4_build_pair_lut(pair_lut);

    if (!test_single_byte_golden()) {
        puts("FAIL four-plane single-byte golden");
        return 1;
    }
    puts("PASS four-plane single-byte golden");

    if (!test_strides_and_arguments()) {
        puts("FAIL four-plane stride and argument contract");
        return 1;
    }
    puts("PASS four-plane stride and argument contract");

    if (!test_profile_equivalence()) {
        puts("FAIL reference, lookup, and mask32 profile equivalence");
        return 1;
    }
    puts("PASS reference, lookup, and mask32 profile equivalence");

    if (!test_canonical_differentials()) {
        puts("FAIL compositor C2P4 decoder differentials");
        return 1;
    }
    puts("PASS compositor C2P4 decoder differentials");
    puts("result=pass");
    return 0;
}
