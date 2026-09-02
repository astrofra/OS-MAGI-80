#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics/c2p_reference.h"

#define TEST_FRAME_WIDTH 256U
#define TEST_FRAME_HEIGHT 256U
#define TEST_CHUNKY_BYTES (TEST_FRAME_WIDTH * TEST_FRAME_HEIGHT)
#define TEST_PACKED_BYTES (TEST_CHUNKY_BYTES / 2U)
#define TEST_PLANE_BYTES (TEST_CHUNKY_BYTES / 8U)

static uint8_t frame_chunky[TEST_CHUNKY_BYTES];
static uint8_t frame_front_packed[TEST_PACKED_BYTES];
static uint8_t frame_back_packed[TEST_PACKED_BYTES];
static uint8_t frame_front_byte[TEST_CHUNKY_BYTES];
static uint8_t frame_back_byte[TEST_CHUNKY_BYTES];
static uint8_t frame_storage[MAGI80_C2P_PLANE_COUNT][TEST_PLANE_BYTES];

static int single_output_matches(
    const uint8_t storage[MAGI80_C2P_PLANE_COUNT],
    const uint8_t expected[MAGI80_C2P_PLANE_COUNT])
{
    return memcmp(storage, expected, MAGI80_C2P_PLANE_COUNT) == 0;
}

static int strided_output_matches(
    const uint8_t storage[MAGI80_C2P_PLANE_COUNT][6])
{
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        uint8_t row0_first = plane == 0U ? 0xffU : 0x00U;
        uint8_t row0_second = plane == 1U ? 0xffU : 0x00U;
        uint8_t row1_first = (plane & 1U) == 0U ? 0xaaU : 0x55U;

        if (storage[plane][0] != row0_first ||
            storage[plane][1] != row0_second ||
            storage[plane][2] != 0x5aU ||
            storage[plane][3] != row1_first ||
            storage[plane][4] != 0xffU ||
            storage[plane][5] != 0x5aU) {
            return 0;
        }
    }
    return 1;
}

static int test_single_byte_vector(void)
{
    static const uint8_t chunky[8] = {
        0xf0U, 0x81U, 0x42U, 0x24U, 0x18U, 0x0fU, 0x5aU, 0xa5U
    };
    static const uint8_t expected[MAGI80_C2P_PLANE_COUNT] = {
        0x8aU, 0x45U, 0x91U, 0x26U, 0xa2U, 0x15U, 0xc1U, 0x0eU
    };
    static const uint8_t front_packed[4] = {
        0xf8U, 0x42U, 0x10U, 0x5aU
    };
    static const uint8_t back_packed[4] = {
        0x01U, 0x24U, 0x8fU, 0xa5U
    };
    static const uint8_t front_byte[8] = {
        0xcfU, 0xc8U, 0xc4U, 0xc2U, 0xc1U, 0xc0U, 0xc5U, 0xcaU
    };
    static const uint8_t back_byte[8] = {
        0xd0U, 0xd1U, 0xd2U, 0xd4U, 0xd8U, 0xdfU, 0xdaU, 0xd5U
    };
    uint8_t storage[MAGI80_C2P_PLANE_COUNT] = {0U};
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = &storage[plane];
    }
    if (magi80_c2p_reference(chunky, 8U, 1U, 8U, planes, 1U) !=
            MAGI80_C2P_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }

    memset(storage, 0, sizeof(storage));
    if (magi80_c2p_reference_packed4_x2(
            front_packed, back_packed, 8U, 1U, 4U, 4U, planes, 1U) !=
            MAGI80_C2P_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }

    memset(storage, 0, sizeof(storage));
    if (magi80_c2p_reference_byte4_x2(
            front_byte, back_byte, 8U, 1U, 8U, 8U, planes, 1U) !=
            MAGI80_C2P_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }

    memset(storage, 0, sizeof(storage));
    if (magi80_c2p_reference_front_byte4_back_packed4(
            front_byte, back_packed, 8U, 1U, 8U, 4U, planes, 1U) !=
            MAGI80_C2P_OK ||
        !single_output_matches(storage, expected)) {
        return 0;
    }
    return 1;
}

static int test_strided_vector(void)
{
    enum {
        WIDTH = 16,
        HEIGHT = 2,
        CHUNKY_STRIDE = 20,
        PLANE_STRIDE = 3
    };
    uint8_t chunky[HEIGHT * CHUNKY_STRIDE];
    uint8_t storage[MAGI80_C2P_PLANE_COUNT][HEIGHT * PLANE_STRIDE];
    uint8_t front_packed[HEIGHT * 9U];
    uint8_t back_packed[HEIGHT * 10U];
    uint8_t front_byte[HEIGHT * 18U];
    uint8_t back_byte[HEIGHT * 19U];
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;
    size_t x;
    size_t y;

    memset(chunky, 0xcc, sizeof(chunky));
    memset(storage, 0x5a, sizeof(storage));
    for (x = 0U; x < 8U; ++x) {
        chunky[x] = 0x10U;
        chunky[8U + x] = 0x01U;
        chunky[CHUNKY_STRIDE + x] = (x & 1U) == 0U ? 0xf0U : 0x0fU;
        chunky[CHUNKY_STRIDE + 8U + x] = 0xffU;
    }
    memset(front_packed, 0xcc, sizeof(front_packed));
    memset(back_packed, 0xcc, sizeof(back_packed));
    memset(front_byte, 0xcc, sizeof(front_byte));
    memset(back_byte, 0xcc, sizeof(back_byte));
    for (y = 0U; y < HEIGHT; ++y) {
        memset(front_packed + (y * 9U), 0, WIDTH / 2U);
        memset(back_packed + (y * 10U), 0, WIDTH / 2U);
        for (x = 0U; x < WIDTH; ++x) {
            uint8_t value = chunky[(y * CHUNKY_STRIDE) + x];
            unsigned int shift = (x & 1U) == 0U ? 4U : 0U;

            front_packed[(y * 9U) + (x >> 1)] |=
                (uint8_t)(((value >> 4) & 0x0fU) << shift);
            back_packed[(y * 10U) + (x >> 1)] |=
                (uint8_t)((value & 0x0fU) << shift);
            front_byte[(y * 18U) + x] =
                (uint8_t)(0xc0U | ((value >> 4) & 0x0fU));
            back_byte[(y * 19U) + x] =
                (uint8_t)(0xd0U | (value & 0x0fU));
        }
    }
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = storage[plane];
    }

    if (magi80_c2p_reference(chunky, WIDTH, HEIGHT, CHUNKY_STRIDE,
                             planes, PLANE_STRIDE) != MAGI80_C2P_OK) {
        return 0;
    }
    if (!strided_output_matches(storage)) {
        return 0;
    }

    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p_reference_packed4_x2(
            front_packed, back_packed, WIDTH, HEIGHT, 9U, 10U, planes,
            PLANE_STRIDE) != MAGI80_C2P_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }

    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p_reference_byte4_x2(
            front_byte, back_byte, WIDTH, HEIGHT, 18U, 19U, planes,
            PLANE_STRIDE) != MAGI80_C2P_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }

    memset(storage, 0x5a, sizeof(storage));
    if (magi80_c2p_reference_front_byte4_back_packed4(
            front_byte, back_packed, WIDTH, HEIGHT, 18U, 10U, planes,
            PLANE_STRIDE) != MAGI80_C2P_OK ||
        !strided_output_matches(storage)) {
        return 0;
    }
    return 1;
}

static int full_frame_matches(
    const uint8_t expected[MAGI80_C2P_PLANE_COUNT])
{
    size_t plane;
    size_t offset;

    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        for (offset = 0U; offset < TEST_PLANE_BYTES; ++offset) {
            if (frame_storage[plane][offset] != expected[plane]) {
                return 0;
            }
        }
    }
    return 1;
}

static int test_full_frame_vector(void)
{
    static const uint8_t expected[MAGI80_C2P_PLANE_COUNT] = {
        0x00U, 0xffU, 0xffU, 0x00U, 0x00U, 0xffU, 0xffU, 0x00U
    };
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;

    memset(frame_chunky, 0xa5, sizeof(frame_chunky));
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = frame_storage[plane];
    }
    memset(frame_storage, 0x00, sizeof(frame_storage));
    if (magi80_c2p_reference(frame_chunky, TEST_FRAME_WIDTH,
                             TEST_FRAME_HEIGHT, TEST_FRAME_WIDTH, planes,
                             TEST_FRAME_WIDTH / 8U) != MAGI80_C2P_OK ||
        !full_frame_matches(expected)) {
        return 0;
    }

    memset(frame_front_packed, 0xaa, sizeof(frame_front_packed));
    memset(frame_back_packed, 0x55, sizeof(frame_back_packed));
    memset(frame_storage, 0x00, sizeof(frame_storage));
    if (magi80_c2p_reference_packed4_x2(
            frame_front_packed, frame_back_packed, TEST_FRAME_WIDTH,
            TEST_FRAME_HEIGHT, TEST_FRAME_WIDTH / 2U,
            TEST_FRAME_WIDTH / 2U, planes, TEST_FRAME_WIDTH / 8U) !=
            MAGI80_C2P_OK ||
        !full_frame_matches(expected)) {
        return 0;
    }

    memset(frame_front_byte, 0xca, sizeof(frame_front_byte));
    memset(frame_back_byte, 0xd5, sizeof(frame_back_byte));
    memset(frame_storage, 0x00, sizeof(frame_storage));
    if (magi80_c2p_reference_byte4_x2(
            frame_front_byte, frame_back_byte, TEST_FRAME_WIDTH,
            TEST_FRAME_HEIGHT, TEST_FRAME_WIDTH, TEST_FRAME_WIDTH, planes,
            TEST_FRAME_WIDTH / 8U) != MAGI80_C2P_OK ||
        !full_frame_matches(expected)) {
        return 0;
    }

    memset(frame_storage, 0x00, sizeof(frame_storage));
    if (magi80_c2p_reference_front_byte4_back_packed4(
            frame_front_byte, frame_back_packed, TEST_FRAME_WIDTH,
            TEST_FRAME_HEIGHT, TEST_FRAME_WIDTH, TEST_FRAME_WIDTH / 2U,
            planes, TEST_FRAME_WIDTH / 8U) != MAGI80_C2P_OK ||
        !full_frame_matches(expected)) {
        return 0;
    }
    return 1;
}

static int test_argument_contract(void)
{
    uint8_t chunky[8] = {0U};
    uint8_t storage[MAGI80_C2P_PLANE_COUNT] = {0U};
    uint8_t packed[4] = {0U};
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = &storage[plane];
    }
    if (magi80_c2p_reference(NULL, 8U, 1U, 8U, planes, 1U) !=
            MAGI80_C2P_INVALID_ARGUMENT ||
        magi80_c2p_reference(chunky, 8U, 1U, 8U, NULL, 1U) !=
            MAGI80_C2P_INVALID_ARGUMENT) {
        return 0;
    }
    planes[7] = NULL;
    if (magi80_c2p_reference(chunky, 8U, 1U, 8U, planes, 1U) !=
        MAGI80_C2P_INVALID_ARGUMENT) {
        return 0;
    }
    planes[7] = &storage[7];
    return magi80_c2p_reference(chunky, 0U, 1U, 8U, planes, 1U) ==
               MAGI80_C2P_INVALID_DIMENSIONS &&
           magi80_c2p_reference(chunky, 7U, 1U, 8U, planes, 1U) ==
               MAGI80_C2P_INVALID_DIMENSIONS &&
           magi80_c2p_reference(chunky, 8U, 0U, 8U, planes, 1U) ==
               MAGI80_C2P_INVALID_DIMENSIONS &&
           magi80_c2p_reference(chunky, 8U, 1U, 7U, planes, 1U) ==
               MAGI80_C2P_INVALID_STRIDE &&
           magi80_c2p_reference(chunky, 16U, 1U, 16U, planes, 1U) ==
               MAGI80_C2P_INVALID_STRIDE &&
           magi80_c2p_reference_packed4_x2(
               packed, packed, 8U, 1U, 3U, 4U, planes, 1U) ==
               MAGI80_C2P_INVALID_STRIDE &&
           magi80_c2p_reference_byte4_x2(
               chunky, chunky, 8U, 1U, 7U, 8U, planes, 1U) ==
               MAGI80_C2P_INVALID_STRIDE &&
           magi80_c2p_reference_front_byte4_back_packed4(
               chunky, packed, 8U, 1U, 8U, 3U, planes, 1U) ==
               MAGI80_C2P_INVALID_STRIDE;
}

int main(void)
{
    if (!test_single_byte_vector()) {
        puts("FAIL single-byte golden vector");
        return 1;
    }
    puts("PASS all-layout single-byte golden vector");

    if (!test_strided_vector()) {
        puts("FAIL strided multi-row golden vector");
        return 1;
    }
    puts("PASS strided multi-row golden vector");

    if (!test_full_frame_vector()) {
        puts("FAIL full-frame golden vector");
        return 1;
    }
    puts("PASS all-layout full-frame golden vector");

    if (!test_argument_contract()) {
        puts("FAIL argument contract");
        return 1;
    }
    puts("PASS argument contract");
    puts("result=pass");
    return 0;
}
