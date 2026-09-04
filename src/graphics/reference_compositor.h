#ifndef MIGA80_GRAPHICS_REFERENCE_COMPOSITOR_H
#define MIGA80_GRAPHICS_REFERENCE_COMPOSITOR_H

#include <stddef.h>
#include <stdint.h>

enum {
    MIGA80_GRAPHICS_REFERENCE_WIDTH = 256,
    MIGA80_GRAPHICS_REFERENCE_HEIGHT = 256,
    MIGA80_GRAPHICS_REFERENCE_MAX_OBJECTS = 256,
    MIGA80_GRAPHICS_PLANAR_COLOR_COUNT = 16,
    MIGA80_GRAPHICS_OVERLAY_COLOR_BASE = 15
};

enum Miga80GraphicsReferenceStatus {
    MIGA80_GRAPHICS_REFERENCE_OK = 0,
    MIGA80_GRAPHICS_REFERENCE_INVALID_ARGUMENT,
    MIGA80_GRAPHICS_REFERENCE_INVALID_DIMENSIONS,
    MIGA80_GRAPHICS_REFERENCE_INVALID_STRIDE,
    MIGA80_GRAPHICS_REFERENCE_INVALID_REGION,
    MIGA80_GRAPHICS_REFERENCE_TOO_MANY_OBJECTS,
    MIGA80_GRAPHICS_REFERENCE_INVALID_BACKEND_HINT
};

/*
 * The backend hint is diagnostic state.  The reference compositor validates
 * it but deliberately ignores it when producing pixels: hardware sprites and
 * planar fallback must have identical logical output.
 */
enum Miga80GraphicsObjectBackendHint {
    MIGA80_GRAPHICS_OBJECT_BACKEND_AUTO = 0,
    MIGA80_GRAPHICS_OBJECT_BACKEND_HARDWARE_SPRITE,
    MIGA80_GRAPHICS_OBJECT_BACKEND_PLANAR_FALLBACK
};

/* Only the low nibble of each source pixel is significant. */
struct Miga80GraphicsReferenceSurface4 {
    const uint8_t *pixels;
    size_t width;
    size_t height;
    size_t stride;
};

/*
 * A view selects a rectangle from a source surface and places it on the
 * 256x256 logical display.  A negative screen position clips the view.
 */
struct Miga80GraphicsReferenceView4 {
    struct Miga80GraphicsReferenceSurface4 surface;
    size_t source_x;
    size_t source_y;
    size_t width;
    size_t height;
    int32_t screen_x;
    int32_t screen_y;
    uint8_t enabled;
};

struct Miga80GraphicsReferenceObject {
    struct Miga80GraphicsReferenceSurface4 surface;
    size_t source_x;
    size_t source_y;
    size_t width;
    size_t height;
    int32_t world_x;
    int32_t world_y;
    uint8_t priority;
    uint8_t visible;
    enum Miga80GraphicsObjectBackendHint backend_hint;
};

struct Miga80GraphicsReferenceScene {
    struct Miga80GraphicsReferenceView4 planar;
    struct Miga80GraphicsReferenceView4 pixel;
    const struct Miga80GraphicsReferenceObject *objects;
    size_t object_count;
    int32_t object_camera_x;
    int32_t object_camera_y;
};

/*
 * Compose PLANAR, PIXEL, and OBJECTS into canonical palette identifiers.
 *
 * Output 0..15 identifies PLANAR colors 0..15.  Output 16..30 identifies
 * opaque PIXEL/OBJECT colors 1..15.  PIXEL and OBJECT index 0 is transparent.
 * Higher object priority is on top; for equal priority, the later list entry
 * is on top.  The complete 256x256 visible output is overwritten on success.
 * Output row padding is preserved.  All input is validated before output is
 * modified.  Input surfaces and output storage must not overlap.
 */
enum Miga80GraphicsReferenceStatus miga80_graphics_reference_compose(
    const struct Miga80GraphicsReferenceScene *scene,
    uint8_t *output,
    size_t output_stride);

#endif
