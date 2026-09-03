#include "graphics/reference_compositor.h"

#include <string.h>

static enum Magi80GraphicsReferenceStatus validate_surface_region(
    const struct Magi80GraphicsReferenceSurface4 *surface,
    size_t source_x,
    size_t source_y,
    size_t width,
    size_t height)
{
    if (surface->pixels == NULL) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_ARGUMENT;
    }
    if (surface->width == 0U || surface->height == 0U || width == 0U ||
        height == 0U || width > MAGI80_GRAPHICS_REFERENCE_WIDTH ||
        height > MAGI80_GRAPHICS_REFERENCE_HEIGHT) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_DIMENSIONS;
    }
    if (surface->stride < surface->width ||
        surface->height > SIZE_MAX / surface->stride) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_STRIDE;
    }
    if (source_x > surface->width || source_y > surface->height ||
        width > surface->width - source_x ||
        height > surface->height - source_y) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_REGION;
    }
    return MAGI80_GRAPHICS_REFERENCE_OK;
}

static enum Magi80GraphicsReferenceStatus validate_view(
    const struct Magi80GraphicsReferenceView4 *view)
{
    if (view->enabled == 0U) {
        return MAGI80_GRAPHICS_REFERENCE_OK;
    }
    return validate_surface_region(&view->surface, view->source_x,
                                   view->source_y, view->width, view->height);
}

static enum Magi80GraphicsReferenceStatus validate_object(
    const struct Magi80GraphicsReferenceObject *object)
{
    enum Magi80GraphicsReferenceStatus status;

    if (object->visible == 0U) {
        return MAGI80_GRAPHICS_REFERENCE_OK;
    }
    if (object->backend_hint != MAGI80_GRAPHICS_OBJECT_BACKEND_AUTO &&
        object->backend_hint !=
            MAGI80_GRAPHICS_OBJECT_BACKEND_HARDWARE_SPRITE &&
        object->backend_hint !=
            MAGI80_GRAPHICS_OBJECT_BACKEND_PLANAR_FALLBACK) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_BACKEND_HINT;
    }
    status = validate_surface_region(&object->surface, object->source_x,
                                     object->source_y, object->width,
                                     object->height);
    return status;
}

static enum Magi80GraphicsReferenceStatus validate_scene(
    const struct Magi80GraphicsReferenceScene *scene,
    const uint8_t *output,
    size_t output_stride)
{
    enum Magi80GraphicsReferenceStatus status;
    size_t object_index;

    if (scene == NULL || output == NULL) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_ARGUMENT;
    }
    if (output_stride < MAGI80_GRAPHICS_REFERENCE_WIDTH ||
        output_stride > SIZE_MAX / MAGI80_GRAPHICS_REFERENCE_HEIGHT) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_STRIDE;
    }
    if (scene->object_count > MAGI80_GRAPHICS_REFERENCE_MAX_OBJECTS) {
        return MAGI80_GRAPHICS_REFERENCE_TOO_MANY_OBJECTS;
    }
    if (scene->object_count != 0U && scene->objects == NULL) {
        return MAGI80_GRAPHICS_REFERENCE_INVALID_ARGUMENT;
    }
    status = validate_view(&scene->planar);
    if (status != MAGI80_GRAPHICS_REFERENCE_OK) {
        return status;
    }
    status = validate_view(&scene->pixel);
    if (status != MAGI80_GRAPHICS_REFERENCE_OK) {
        return status;
    }
    for (object_index = 0U; object_index < scene->object_count;
         ++object_index) {
        status = validate_object(&scene->objects[object_index]);
        if (status != MAGI80_GRAPHICS_REFERENCE_OK) {
            return status;
        }
    }
    return MAGI80_GRAPHICS_REFERENCE_OK;
}

static uint8_t canonical_overlay_color(uint8_t source)
{
    return (uint8_t)(MAGI80_GRAPHICS_OVERLAY_COLOR_BASE + source);
}

static void draw_view(const struct Magi80GraphicsReferenceView4 *view,
                      uint8_t *output, size_t output_stride, int overlay)
{
    size_t local_y;

    if (view->enabled == 0U) {
        return;
    }
    for (local_y = 0U; local_y < view->height; ++local_y) {
        int64_t display_y = (int64_t)view->screen_y + (int64_t)local_y;
        const uint8_t *source_row;
        uint8_t *output_row;
        size_t local_x;

        if (display_y < 0 ||
            display_y >= MAGI80_GRAPHICS_REFERENCE_HEIGHT) {
            continue;
        }
        source_row = view->surface.pixels +
                     ((view->source_y + local_y) * view->surface.stride) +
                     view->source_x;
        output_row = output + ((size_t)display_y * output_stride);

        for (local_x = 0U; local_x < view->width; ++local_x) {
            int64_t display_x = (int64_t)view->screen_x + (int64_t)local_x;
            uint8_t source;

            if (display_x < 0 ||
                display_x >= MAGI80_GRAPHICS_REFERENCE_WIDTH) {
                continue;
            }
            source = (uint8_t)(source_row[local_x] & 0x0fU);
            if (overlay != 0) {
                if (source != 0U) {
                    output_row[(size_t)display_x] =
                        canonical_overlay_color(source);
                }
            } else {
                output_row[(size_t)display_x] = source;
            }
        }
    }
}

static void draw_object(
    const struct Magi80GraphicsReferenceObject *object,
    int32_t camera_x,
    int32_t camera_y,
    uint8_t *output,
    size_t output_stride)
{
    int64_t origin_x = (int64_t)object->world_x - (int64_t)camera_x;
    int64_t origin_y = (int64_t)object->world_y - (int64_t)camera_y;
    size_t local_y;

    for (local_y = 0U; local_y < object->height; ++local_y) {
        int64_t display_y = origin_y + (int64_t)local_y;
        const uint8_t *source_row;
        uint8_t *output_row;
        size_t local_x;

        if (display_y < 0 ||
            display_y >= MAGI80_GRAPHICS_REFERENCE_HEIGHT) {
            continue;
        }
        source_row = object->surface.pixels +
                     ((object->source_y + local_y) *
                      object->surface.stride) +
                     object->source_x;
        output_row = output + ((size_t)display_y * output_stride);

        for (local_x = 0U; local_x < object->width; ++local_x) {
            int64_t display_x = origin_x + (int64_t)local_x;
            uint8_t source;

            if (display_x < 0 ||
                display_x >= MAGI80_GRAPHICS_REFERENCE_WIDTH) {
                continue;
            }
            source = (uint8_t)(source_row[local_x] & 0x0fU);
            if (source != 0U) {
                output_row[(size_t)display_x] =
                    canonical_overlay_color(source);
            }
        }
    }
}

enum Magi80GraphicsReferenceStatus magi80_graphics_reference_compose(
    const struct Magi80GraphicsReferenceScene *scene,
    uint8_t *output,
    size_t output_stride)
{
    enum Magi80GraphicsReferenceStatus status;
    size_t y;
    unsigned int priority;

    status = validate_scene(scene, output, output_stride);
    if (status != MAGI80_GRAPHICS_REFERENCE_OK) {
        return status;
    }

    for (y = 0U; y < MAGI80_GRAPHICS_REFERENCE_HEIGHT; ++y) {
        memset(output + (y * output_stride), 0,
               MAGI80_GRAPHICS_REFERENCE_WIDTH);
    }
    draw_view(&scene->planar, output, output_stride, 0);
    draw_view(&scene->pixel, output, output_stride, 1);

    for (priority = 0U; priority <= UINT8_MAX; ++priority) {
        size_t object_index;

        for (object_index = 0U; object_index < scene->object_count;
             ++object_index) {
            const struct Magi80GraphicsReferenceObject *object =
                &scene->objects[object_index];

            if (object->visible != 0U &&
                object->priority == (uint8_t)priority) {
                draw_object(object, scene->object_camera_x,
                            scene->object_camera_y, output, output_stride);
            }
        }
    }
    return MAGI80_GRAPHICS_REFERENCE_OK;
}
