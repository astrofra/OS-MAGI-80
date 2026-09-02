#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics/c2p_reference.h"

#define TEST_FRAME_WIDTH 256U
#define TEST_FRAME_HEIGHT 256U
#define TEST_CHUNKY_BYTES (TEST_FRAME_WIDTH * TEST_FRAME_HEIGHT)
#define TEST_PLANE_BYTES (TEST_CHUNKY_BYTES / 8U)

static uint8_t frame_chunky[TEST_CHUNKY_BYTES];
static uint8_t frame_storage[MAGI80_C2P_PLANE_COUNT][TEST_PLANE_BYTES];

static int test_single_byte_vector(void)
{
    static const uint8_t chunky[8] = {
        0xf0U, 0x81U, 0x42U, 0x24U, 0x18U, 0x0fU, 0x5aU, 0xa5U
    };
    static const uint8_t expected[MAGI80_C2P_PLANE_COUNT] = {
        0x8aU, 0x45U, 0x91U, 0x26U, 0xa2U, 0x15U, 0xc1U, 0x0eU
    };
    uint8_t storage[MAGI80_C2P_PLANE_COUNT] = {0U};
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;

    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = &storage[plane];
    }
    if (magi80_c2p_reference(chunky, 8U, 1U, 8U, planes, 1U) !=
        MAGI80_C2P_OK) {
        return 0;
    }
    return memcmp(storage, expected, sizeof(expected)) == 0;
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
    uint8_t *planes[MAGI80_C2P_PLANE_COUNT];
    size_t plane;
    size_t x;

    memset(chunky, 0xcc, sizeof(chunky));
    memset(storage, 0x5a, sizeof(storage));
    for (x = 0U; x < 8U; ++x) {
        chunky[x] = 0x10U;
        chunky[8U + x] = 0x01U;
        chunky[CHUNKY_STRIDE + x] = (x & 1U) == 0U ? 0xf0U : 0x0fU;
        chunky[CHUNKY_STRIDE + 8U + x] = 0xffU;
    }
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = storage[plane];
    }

    if (magi80_c2p_reference(chunky, WIDTH, HEIGHT, CHUNKY_STRIDE,
                             planes, PLANE_STRIDE) != MAGI80_C2P_OK) {
        return 0;
    }
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        uint8_t row0_first = plane == 0U ? 0xffU : 0x00U;
        uint8_t row0_second = plane == 1U ? 0xffU : 0x00U;
        uint8_t row1_first = (plane & 1U) == 0U ? 0xaaU : 0x55U;

        if (storage[plane][0] != row0_first ||
            storage[plane][1] != row0_second ||
            storage[plane][2] != 0x5aU ||
            storage[plane][PLANE_STRIDE] != row1_first ||
            storage[plane][PLANE_STRIDE + 1U] != 0xffU ||
            storage[plane][PLANE_STRIDE + 2U] != 0x5aU) {
            return 0;
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
    size_t offset;

    memset(frame_chunky, 0xa5, sizeof(frame_chunky));
    memset(frame_storage, 0x00, sizeof(frame_storage));
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        planes[plane] = frame_storage[plane];
    }
    if (magi80_c2p_reference(frame_chunky, TEST_FRAME_WIDTH,
                             TEST_FRAME_HEIGHT, TEST_FRAME_WIDTH, planes,
                             TEST_FRAME_WIDTH / 8U) != MAGI80_C2P_OK) {
        return 0;
    }
    for (plane = 0U; plane < MAGI80_C2P_PLANE_COUNT; ++plane) {
        for (offset = 0U; offset < TEST_PLANE_BYTES; ++offset) {
            if (frame_storage[plane][offset] != expected[plane]) {
                return 0;
            }
        }
    }
    return 1;
}

static int test_argument_contract(void)
{
    uint8_t chunky[8] = {0U};
    uint8_t storage[MAGI80_C2P_PLANE_COUNT] = {0U};
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
               MAGI80_C2P_INVALID_STRIDE;
}

int main(void)
{
    if (!test_single_byte_vector()) {
        puts("FAIL single-byte golden vector");
        return 1;
    }
    puts("PASS single-byte golden vector");

    if (!test_strided_vector()) {
        puts("FAIL strided multi-row golden vector");
        return 1;
    }
    puts("PASS strided multi-row golden vector");

    if (!test_full_frame_vector()) {
        puts("FAIL full-frame golden vector");
        return 1;
    }
    puts("PASS full-frame golden vector");

    if (!test_argument_contract()) {
        puts("FAIL argument contract");
        return 1;
    }
    puts("PASS argument contract");
    puts("result=pass");
    return 0;
}
