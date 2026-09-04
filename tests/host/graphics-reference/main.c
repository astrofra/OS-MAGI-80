#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "graphics/reference_compositor.h"

#define OUTPUT_STRIDE 260U
#define OUTPUT_BYTES (OUTPUT_STRIDE * MIGA80_GRAPHICS_REFERENCE_HEIGHT)
#define VISIBLE_BYTES                                                   \
    (MIGA80_GRAPHICS_REFERENCE_WIDTH * MIGA80_GRAPHICS_REFERENCE_HEIGHT)

static uint8_t planar_full[VISIBLE_BYTES];
static uint8_t output[OUTPUT_BYTES];
static uint8_t output_second[OUTPUT_BYTES];
static uint8_t expected_visible[VISIBLE_BYTES];

static uint8_t pixel_at(const uint8_t *storage, size_t x, size_t y)
{
    return storage[(y * OUTPUT_STRIDE) + x];
}

static int visible_outputs_match(const uint8_t *left, const uint8_t *right)
{
    size_t y;

    for (y = 0U; y < MIGA80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        if (memcmp(left + (y * OUTPUT_STRIDE),
                   right + (y * OUTPUT_STRIDE),
                   MIGA80_GRAPHICS_REFERENCE_WIDTH) != 0) {
            return 0;
        }
    }
    return 1;
}

static int padding_is(const uint8_t *storage, uint8_t expected)
{
    size_t y;
    size_t x;

    for (y = 0U; y < MIGA80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        for (x = MIGA80_GRAPHICS_REFERENCE_WIDTH; x < OUTPUT_STRIDE; ++x) {
            if (storage[(y * OUTPUT_STRIDE) + x] != expected) {
                return 0;
            }
        }
    }
    return 1;
}

static void set_surface(struct Miga80GraphicsReferenceSurface4 *surface,
                        const uint8_t *pixels, size_t width, size_t height,
                        size_t stride)
{
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = stride;
}

static void set_view(struct Miga80GraphicsReferenceView4 *view,
                     const uint8_t *pixels, size_t surface_width,
                     size_t surface_height, size_t stride, size_t source_x,
                     size_t source_y, size_t width, size_t height,
                     int32_t screen_x, int32_t screen_y)
{
    memset(view, 0, sizeof(*view));
    set_surface(&view->surface, pixels, surface_width, surface_height,
                stride);
    view->source_x = source_x;
    view->source_y = source_y;
    view->width = width;
    view->height = height;
    view->screen_x = screen_x;
    view->screen_y = screen_y;
    view->enabled = 1U;
}

static void set_object(
    struct Miga80GraphicsReferenceObject *object, const uint8_t *pixels,
    size_t surface_width, size_t surface_height, size_t stride,
    size_t source_x, size_t source_y, size_t width, size_t height,
    int32_t world_x, int32_t world_y, uint8_t priority,
    enum Miga80GraphicsObjectBackendHint backend_hint)
{
    memset(object, 0, sizeof(*object));
    set_surface(&object->surface, pixels, surface_width, surface_height,
                stride);
    object->source_x = source_x;
    object->source_y = source_y;
    object->width = width;
    object->height = height;
    object->world_x = world_x;
    object->world_y = world_y;
    object->priority = priority;
    object->visible = 1U;
    object->backend_hint = backend_hint;
}

static void setup_golden_scene(
    struct Miga80GraphicsReferenceScene *scene,
    struct Miga80GraphicsReferenceObject objects[3])
{
    static uint8_t pixel[4][8];
    static uint8_t object_low[2][4];
    static uint8_t object_high[3][3];
    static uint8_t object_tie[1][2];
    static const uint8_t pixel_values[4][6] = {
        {0U, 2U, 0U, 3U, 0U, 4U},
        {5U, 0U, 6U, 0U, 7U, 0U},
        {0U, 8U, 0U, 9U, 0U, 10U},
        {11U, 0U, 12U, 0U, 13U, 0U}
    };
    static const uint8_t object_low_values[2][3] = {
        {1U, 1U, 1U},
        {1U, 0U, 1U}
    };
    static const uint8_t object_high_values[3][2] = {
        {2U, 0U},
        {2U, 2U},
        {0U, 2U}
    };
    size_t x;
    size_t y;

    memset(scene, 0, sizeof(*scene));
    memset(planar_full, 0xa1, sizeof(planar_full));
    memset(pixel, 0xee, sizeof(pixel));
    memset(object_low, 0xee, sizeof(object_low));
    memset(object_high, 0xee, sizeof(object_high));
    memset(object_tie, 0xee, sizeof(object_tie));

    for (y = 0U; y < 4U; ++y) {
        for (x = 0U; x < 6U; ++x) {
            pixel[y][x] = (uint8_t)(0xb0U | pixel_values[y][x]);
        }
    }
    for (y = 0U; y < 2U; ++y) {
        for (x = 0U; x < 3U; ++x) {
            object_low[y][x] =
                (uint8_t)(0xc0U | object_low_values[y][x]);
        }
    }
    for (y = 0U; y < 3U; ++y) {
        for (x = 0U; x < 2U; ++x) {
            object_high[y][x] =
                (uint8_t)(0xd0U | object_high_values[y][x]);
        }
    }
    object_tie[0][0] = 0xe3U;

    set_view(&scene->planar, planar_full,
             MIGA80_GRAPHICS_REFERENCE_WIDTH,
             MIGA80_GRAPHICS_REFERENCE_HEIGHT,
             MIGA80_GRAPHICS_REFERENCE_WIDTH, 0U, 0U,
             MIGA80_GRAPHICS_REFERENCE_WIDTH,
             MIGA80_GRAPHICS_REFERENCE_HEIGHT, 0, 0);
    set_view(&scene->pixel, &pixel[0][0], 6U, 4U, 8U, 0U, 0U, 6U, 4U,
             1, 1);

    set_object(&objects[0], &object_low[0][0], 3U, 2U, 4U, 0U, 0U,
               3U, 2U, 2, 2, 1U,
               MIGA80_GRAPHICS_OBJECT_BACKEND_HARDWARE_SPRITE);
    set_object(&objects[1], &object_high[0][0], 2U, 3U, 3U, 0U, 0U,
               2U, 3U, 3, 1, 2U,
               MIGA80_GRAPHICS_OBJECT_BACKEND_PLANAR_FALLBACK);
    set_object(&objects[2], &object_tie[0][0], 1U, 1U, 2U, 0U, 0U,
               1U, 1U, 3, 2, 2U,
               MIGA80_GRAPHICS_OBJECT_BACKEND_AUTO);
    scene->objects = objects;
    scene->object_count = 3U;
}

static int test_golden_composition(void)
{
    static const uint8_t expected[6][8] = {
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U},
        {1U, 1U, 17U, 17U, 18U, 1U, 19U, 1U},
        {1U, 20U, 16U, 18U, 17U, 22U, 1U, 1U},
        {1U, 1U, 16U, 1U, 17U, 1U, 25U, 1U},
        {1U, 26U, 1U, 27U, 1U, 28U, 1U, 1U},
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U}
    };
    struct Miga80GraphicsReferenceScene scene;
    struct Miga80GraphicsReferenceObject objects[3];
    size_t x;
    size_t y;

    setup_golden_scene(&scene, objects);
    memset(output, 0xcc, sizeof(output));
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_OK) {
        return 0;
    }
    for (y = 0U; y < MIGA80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        for (x = 0U; x < MIGA80_GRAPHICS_REFERENCE_WIDTH; ++x) {
            uint8_t wanted = y < 6U && x < 8U ? expected[y][x] : 1U;

            if (pixel_at(output, x, y) != wanted) {
                return 0;
            }
        }
    }
    return padding_is(output, 0xccU);
}

static int test_backend_path_equivalence(void)
{
    struct Miga80GraphicsReferenceScene scene;
    struct Miga80GraphicsReferenceObject objects[3];

    setup_golden_scene(&scene, objects);
    memset(output, 0xcc, sizeof(output));
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_OK) {
        return 0;
    }

    objects[0].backend_hint =
        MIGA80_GRAPHICS_OBJECT_BACKEND_PLANAR_FALLBACK;
    objects[1].backend_hint =
        MIGA80_GRAPHICS_OBJECT_BACKEND_HARDWARE_SPRITE;
    objects[2].backend_hint =
        MIGA80_GRAPHICS_OBJECT_BACKEND_HARDWARE_SPRITE;
    memset(output_second, 0xdd, sizeof(output_second));
    if (miga80_graphics_reference_compose(
            &scene, output_second, OUTPUT_STRIDE) !=
            MIGA80_GRAPHICS_REFERENCE_OK ||
        !visible_outputs_match(output, output_second) ||
        !padding_is(output_second, 0xddU)) {
        return 0;
    }
    return 1;
}

static int test_origins_clipping_and_camera(void)
{
    uint8_t planar[4][7];
    uint8_t pixel[3][7];
    uint8_t object_pixels[2][5];
    struct Miga80GraphicsReferenceScene scene;
    struct Miga80GraphicsReferenceObject object;
    size_t x;
    size_t y;

    memset(&scene, 0, sizeof(scene));
    memset(planar, 0xee, sizeof(planar));
    memset(pixel, 0xee, sizeof(pixel));
    memset(object_pixels, 0xee, sizeof(object_pixels));
    for (y = 0U; y < 4U; ++y) {
        for (x = 0U; x < 5U; ++x) {
            planar[y][x] = (uint8_t)(0xa0U | ((y * 5U + x) & 0x0fU));
        }
    }
    pixel[1][1] = 0xb6U;
    pixel[1][2] = 0xb0U;
    pixel[1][3] = 0xb7U;
    pixel[1][4] = 0xb8U;
    pixel[2][1] = 0xb9U;
    pixel[2][2] = 0xbaU;
    pixel[2][3] = 0xb0U;
    pixel[2][4] = 0xbbU;
    object_pixels[0][0] = 0xc1U;
    object_pixels[0][1] = 0xc2U;
    object_pixels[0][2] = 0xc3U;
    object_pixels[1][0] = 0xc4U;
    object_pixels[1][1] = 0xc0U;
    object_pixels[1][2] = 0xc5U;

    set_view(&scene.planar, &planar[0][0], 5U, 4U, 7U, 1U, 1U, 4U,
             3U, -1, -1);
    set_view(&scene.pixel, &pixel[0][0], 5U, 3U, 7U, 1U, 1U, 4U, 2U,
             254, 0);
    set_object(&object, &object_pixels[0][0], 3U, 2U, 5U, 0U, 0U, 3U,
               2U, 2, 2, 3U, MIGA80_GRAPHICS_OBJECT_BACKEND_AUTO);
    scene.objects = &object;
    scene.object_count = 1U;
    scene.object_camera_x = 3;
    scene.object_camera_y = 1;

    memset(expected_visible, 0, sizeof(expected_visible));
    expected_visible[0U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 0U] = 12U;
    expected_visible[0U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 1U] = 13U;
    expected_visible[0U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 2U] = 14U;
    expected_visible[1U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 0U] = 17U;
    expected_visible[1U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 1U] = 18U;
    expected_visible[1U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 2U] = 3U;
    expected_visible[2U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 1U] = 20U;
    expected_visible[0U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 254U] = 21U;
    expected_visible[1U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 254U] = 24U;
    expected_visible[1U * MIGA80_GRAPHICS_REFERENCE_WIDTH + 255U] = 25U;

    memset(output, 0xcc, sizeof(output));
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_OK) {
        return 0;
    }
    for (y = 0U; y < MIGA80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        for (x = 0U; x < MIGA80_GRAPHICS_REFERENCE_WIDTH; ++x) {
            if (pixel_at(output, x, y) !=
                expected_visible[y * MIGA80_GRAPHICS_REFERENCE_WIDTH + x]) {
                return 0;
            }
        }
    }
    return padding_is(output, 0xccU);
}

static int test_argument_contract(void)
{
    uint8_t source[4] = {0U};
    struct Miga80GraphicsReferenceScene scene;
    struct Miga80GraphicsReferenceObject object;

    memset(&scene, 0, sizeof(scene));
    memset(output, 0x5a, sizeof(output));
    if (miga80_graphics_reference_compose(NULL, output, OUTPUT_STRIDE) !=
            MIGA80_GRAPHICS_REFERENCE_INVALID_ARGUMENT ||
        miga80_graphics_reference_compose(&scene, NULL, OUTPUT_STRIDE) !=
            MIGA80_GRAPHICS_REFERENCE_INVALID_ARGUMENT ||
        miga80_graphics_reference_compose(
            &scene, output, MIGA80_GRAPHICS_REFERENCE_WIDTH - 1U) !=
            MIGA80_GRAPHICS_REFERENCE_INVALID_STRIDE) {
        return 0;
    }

    scene.object_count = 1U;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_ARGUMENT) {
        return 0;
    }
    scene.object_count = MIGA80_GRAPHICS_REFERENCE_MAX_OBJECTS + 1U;
    scene.objects = &object;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_TOO_MANY_OBJECTS) {
        return 0;
    }

    memset(&scene, 0, sizeof(scene));
    set_view(&scene.planar, source, 2U, 2U, 2U, 0U, 0U, 2U, 2U, 0,
             0);
    scene.planar.surface.pixels = NULL;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_ARGUMENT) {
        return 0;
    }
    scene.planar.surface.pixels = source;
    scene.planar.surface.stride = 1U;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_STRIDE) {
        return 0;
    }
    scene.planar.surface.stride = 2U;
    scene.planar.width = 0U;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_DIMENSIONS) {
        return 0;
    }
    scene.planar.width = 2U;
    scene.planar.source_x = 1U;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_REGION) {
        return 0;
    }

    memset(&scene, 0, sizeof(scene));
    set_object(&object, source, 2U, 2U, 2U, 0U, 0U, 2U, 2U, 0, 0,
               0U, MIGA80_GRAPHICS_OBJECT_BACKEND_AUTO);
    scene.objects = &object;
    scene.object_count = 1U;
    object.backend_hint = (enum Miga80GraphicsObjectBackendHint)99;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_BACKEND_HINT) {
        return 0;
    }
    object.backend_hint = MIGA80_GRAPHICS_OBJECT_BACKEND_AUTO;
    object.width = MIGA80_GRAPHICS_REFERENCE_WIDTH + 1U;
    if (miga80_graphics_reference_compose(&scene, output, OUTPUT_STRIDE) !=
        MIGA80_GRAPHICS_REFERENCE_INVALID_DIMENSIONS) {
        return 0;
    }

    return output[0] == 0x5aU;
}

int main(void)
{
    if (!test_golden_composition()) {
        puts("FAIL three-layer golden composition");
        return 1;
    }
    puts("PASS three-layer golden composition");

    if (!test_backend_path_equivalence()) {
        puts("FAIL object backend-path equivalence");
        return 1;
    }
    puts("PASS object backend-path equivalence");

    if (!test_origins_clipping_and_camera()) {
        puts("FAIL source origins, clipping, and object camera");
        return 1;
    }
    puts("PASS source origins, clipping, and object camera");

    if (!test_argument_contract()) {
        puts("FAIL argument contract");
        return 1;
    }
    puts("PASS argument contract");
    puts("result=pass");
    return 0;
}
