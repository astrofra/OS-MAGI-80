#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <devices/timer.h>
#include <dos/dos.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <graphics/videocontrol.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <utility/tagitem.h>

#include "graphics/aga_reference_decoder.h"
#include "graphics/c2p4_reference.h"
#include "graphics/reference_compositor.h"

#define BENCH_SCREEN_WIDTH 256U
#define BENCH_SCREEN_HEIGHT 256U
#define BENCH_SCREEN_PIXELS (BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT)
#define BENCH_SCREEN_PLANE_BYTES (BENCH_SCREEN_PIXELS / 8U)
#define BENCH_DISPLAY_BYTES \
    (BENCH_SCREEN_PLANE_BYTES * MAGI80_AGA_REFERENCE_PLANE_COUNT)
#define BENCH_DISPLAY_ID (PAL_MONITOR_ID | LORESDPF_KEY)
#define BENCH_SAMPLES 3U
#define BENCH_PROFILE_COUNT 3U
#define BENCH_LAYOUT_COUNT 2U
#define BENCH_BACKEND_COUNT 4U
#define BENCH_CASE_COUNT \
    (BENCH_PROFILE_COUNT * BENCH_LAYOUT_COUNT * BENCH_BACKEND_COUNT)

#ifndef MAGI80_BENCHMARK_ENVIRONMENT
#define MAGI80_BENCHMARK_ENVIRONMENT "fs_uae_a1200_pal"
#endif

#ifndef MAGI80_BENCHMARK_TIMING_AUTHORITY
#define MAGI80_BENCHMARK_TIMING_AUTHORITY "protocol_only"
#endif

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Device *TimerBase = NULL;

enum SourceLayout {
    SOURCE_PACKED4 = 0,
    SOURCE_BYTE4
};

enum CpuBackend {
    BACKEND_SCALAR_C99 = 0,
    BACKEND_PAIR_LUT_C99,
    BACKEND_PAIR_LUT_M68K,
    BACKEND_MASK32_M68K,
    BACKEND_PAIR_LUT_M68K_BLIT
};

struct ViewportProfile {
    const char *name;
    ULONG width;
    ULONG height;
};

struct BenchmarkResult {
    const char *profile_name;
    const char *layout_name;
    const char *backend_name;
    const char *blitter_role;
    const char *publication_mode;
    ULONG width;
    ULONG height;
    ULONG samples;
    ULONG memory_bytes;
    ULONG dirty_bytes;
    ULONG dirty_regions;
    ULONG objects;
    ULONG fallback_objects;
    ULONG frame_budget_ticks;
    ULONG source_median_ticks;
    ULONG draw_median_ticks;
    ULONG cpu_conversion_median_ticks;
    ULONG blitter_median_ticks;
    ULONG blitter_wait_median_ticks;
    ULONG publication_median_ticks;
    ULONG total_min_ticks;
    ULONG total_median_ticks;
    ULONG total_max_ticks;
    ULONG oracle_checksum;
    ULONG canonical_checksum;
    ULONG deadline_misses;
    ULONG source_bytes;
    ULONG scratch_bytes;
    ULONG psets;
    ULONG lookup_traffic_bytes;
    ULONG minimum_chip_traffic_bytes;
};

static const struct ViewportProfile profiles[BENCH_PROFILE_COUNT] = {
    {"160x128", 160U, 128U},
    {"192x160", 192U, 160U},
    {"256x256", 256U, 256U}
};

static const enum CpuBackend benchmark_backends[BENCH_BACKEND_COUNT] = {
    BACKEND_PAIR_LUT_C99,
    BACKEND_PAIR_LUT_M68K,
    BACKEND_MASK32_M68K,
    BACKEND_PAIR_LUT_M68K_BLIT
};

static uint32_t pair_lut[MAGI80_C2P4_PAIR_LUT_ENTRIES];

static int write_bytes(BPTR output, const char *text, size_t length)
{
    if (output == (BPTR)0 || length > 0x7fffffffUL) {
        return 0;
    }
    return Write(output, text, (LONG)length) == (LONG)length;
}

static int write_text(BPTR output, const char *text)
{
    const char *end = text;

    while (*end != '\0') {
        ++end;
    }
    return write_bytes(output, text, (size_t)(end - text));
}

static int write_decimal(BPTR output, ULONG value)
{
    char digits[10];
    size_t count = 0U;

    do {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));

    while (count > 0U) {
        --count;
        if (!write_bytes(output, &digits[count], 1U)) {
            return 0;
        }
    }
    return 1;
}

static int write_key_decimal(BPTR output, const char *key, ULONG value)
{
    return write_text(output, key) && write_decimal(output, value);
}

static int report_failure(const char *stage)
{
    BPTR output = Output();

    if (!write_text(output, "failure=") || !write_text(output, stage) ||
        !write_text(output, "\nresult=fail\n")) {
        return RETURN_ERROR;
    }
    return RETURN_FAIL;
}

static ULONG elapsed_ticks(const struct EClockVal *start,
                           const struct EClockVal *end)
{
    return end->ev_lo - start->ev_lo;
}

static ULONG timer_overhead(void)
{
    struct EClockVal start;
    struct EClockVal end;
    ULONG minimum = 0xffffffffUL;
    ULONG attempt;

    for (attempt = 0U; attempt < 16U; ++attempt) {
        (void)ReadEClock(&start);
        (void)ReadEClock(&end);
        if (elapsed_ticks(&start, &end) < minimum) {
            minimum = elapsed_ticks(&start, &end);
        }
    }
    return minimum;
}

static uint8_t source_color(ULONG x, ULONG y)
{
    uint8_t color =
        (uint8_t)(((x * 3U) + (y * 5U) + (x ^ y)) & 0x0fU);

    if (((x + (y * 3U)) & 7U) < 3U) {
        color = 0U;
    }
    return color;
}

static uint8_t planar_color(ULONG x, ULONG y)
{
    return (uint8_t)(((x >> 3) + (y >> 2) + 7U) & 0x0fU);
}

static void build_source(uint8_t *source, enum SourceLayout layout,
                         ULONG width, ULONG height)
{
    ULONG x;
    ULONG y;

    for (y = 0U; y < height; ++y) {
        if (layout == SOURCE_PACKED4) {
            for (x = 0U; x < width; x += 2U) {
                source[(y * (width >> 1)) + (x >> 1)] =
                    (uint8_t)((source_color(x, y) << 4) |
                              source_color(x + 1U, y));
            }
        } else {
            for (x = 0U; x < width; ++x) {
                source[(y * width) + x] =
                    (uint8_t)(0xa0U | source_color(x, y));
            }
        }
    }
}

static uint8_t draw_color(ULONG count)
{
    return (uint8_t)(((count >> 2) ^ (count >> 7) ^ 0x0dU) & 0x0fU);
}

static void draw_psets(uint8_t *source, enum SourceLayout layout,
                       ULONG pixels, ULONG psets)
{
    ULONG index = 17U;
    ULONG count;

    for (count = 0U; count < psets; ++count) {
        uint8_t color = draw_color(count);

        index += 4051U;
        if (index >= pixels) {
            index -= pixels;
        }
        if (layout == SOURCE_PACKED4) {
            uint8_t *destination = &source[index >> 1];

            if ((index & 1U) == 0U) {
                *destination =
                    (uint8_t)((*destination & 0x0fU) | (color << 4));
            } else {
                *destination =
                    (uint8_t)((*destination & 0xf0U) | color);
            }
        } else {
            source[index] = (uint8_t)(0xb0U | color);
        }
    }
}

static void unpack_source(const uint8_t *source, enum SourceLayout layout,
                          ULONG width, ULONG height, uint8_t *unpacked)
{
    ULONG x;
    ULONG y;

    if (layout == SOURCE_BYTE4) {
        ULONG pixel;

        for (pixel = 0U; pixel < width * height; ++pixel) {
            unpacked[pixel] = (uint8_t)(source[pixel] & 0x0fU);
        }
        return;
    }
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            uint8_t pair = source[(y * (width >> 1)) + (x >> 1)];
            unsigned int shift = (x & 1U) == 0U ? 4U : 0U;

            unpacked[(y * width) + x] =
                (uint8_t)((pair >> shift) & 0x0fU);
        }
    }
}

static ULONG fnv1a32(const uint8_t *bytes, ULONG length)
{
    ULONG hash = 2166136261UL;
    ULONG offset;

    for (offset = 0U; offset < length; ++offset) {
        hash ^= bytes[offset];
        hash *= 16777619UL;
    }
    return hash;
}

static void sort_ticks(ULONG values[BENCH_SAMPLES])
{
    ULONG position;

    for (position = 1U; position < BENCH_SAMPLES; ++position) {
        ULONG value = values[position];
        ULONG insertion = position;

        while (insertion > 0U && values[insertion - 1U] > value) {
            values[insertion] = values[insertion - 1U];
            --insertion;
        }
        values[insertion] = value;
    }
}

static ULONG median_ticks(ULONG values[BENCH_SAMPLES])
{
    sort_ticks(values);
    return values[BENCH_SAMPLES / 2U];
}

static void clear_front_planes(
    uint8_t *physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride)
{
    size_t logical_plane;

    for (logical_plane = 0U;
         logical_plane < MAGI80_C2P4_PLANE_COUNT; ++logical_plane) {
        memset(physical_planes[logical_plane << 1], 0,
               plane_stride * BENCH_SCREEN_HEIGHT);
    }
}

static enum Magi80C2P4Status convert_source(
    const uint8_t *source,
    enum SourceLayout layout,
    enum CpuBackend backend,
    ULONG width,
    ULONG height,
    uint8_t *planes[MAGI80_C2P4_PLANE_COUNT],
    size_t plane_stride)
{
    size_t source_stride =
        layout == SOURCE_PACKED4 ? (size_t)(width >> 1) : (size_t)width;

    if (backend == BACKEND_SCALAR_C99) {
        if (layout == SOURCE_PACKED4) {
            return magi80_c2p4_reference_packed4(
                source, width, height, source_stride, planes,
                plane_stride);
        }
        return magi80_c2p4_reference_byte4(
            source, width, height, source_stride, planes, plane_stride);
    }
    if (backend == BACKEND_MASK32_M68K) {
        if (layout == SOURCE_PACKED4) {
            return magi80_c2p4_mask32_m68k_packed4(
                source, width, height, source_stride, planes,
                plane_stride);
        }
        return magi80_c2p4_mask32_m68k_byte4(
            source, width, height, source_stride, planes, plane_stride);
    }
    if (layout == SOURCE_PACKED4) {
        if (backend == BACKEND_PAIR_LUT_C99) {
            return magi80_c2p4_lookup_packed4(
                source, width, height, source_stride, planes, plane_stride,
                pair_lut);
        }
        return magi80_c2p4_pair_lut_m68k_packed4(
            source, width, height, source_stride, planes, plane_stride,
            pair_lut);
    }
    if (backend == BACKEND_PAIR_LUT_C99) {
        return magi80_c2p4_lookup_byte4(
            source, width, height, source_stride, planes, plane_stride,
            pair_lut);
    }
    return magi80_c2p4_pair_lut_m68k_byte4(
        source, width, height, source_stride, planes, plane_stride,
        pair_lut);
}

static int prepare_planar_base(
    uint8_t *planar_source,
    uint8_t *physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride)
{
    uint8_t *back_planes[MAGI80_C2P4_PLANE_COUNT];
    ULONG x;
    ULONG y;
    size_t plane;

    for (y = 0U; y < BENCH_SCREEN_HEIGHT; ++y) {
        for (x = 0U; x < BENCH_SCREEN_WIDTH; ++x) {
            planar_source[(y * BENCH_SCREEN_WIDTH) + x] =
                planar_color(x, y);
        }
    }
    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        back_planes[plane] = physical_planes[(plane << 1) + 1U];
    }
    return magi80_c2p4_reference_byte4(
               planar_source, BENCH_SCREEN_WIDTH, BENCH_SCREEN_HEIGHT,
               BENCH_SCREEN_WIDTH, back_planes, plane_stride) ==
           MAGI80_C2P4_OK;
}

static int verify_case(
    const uint8_t *source,
    enum SourceLayout layout,
    const struct ViewportProfile *profile,
    const uint8_t *planar_source,
    uint8_t *oracle_front,
    uint8_t *canonical_expected,
    uint8_t *canonical_actual,
    uint8_t *physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride,
    ULONG *oracle_checksum,
    ULONG *canonical_checksum)
{
    struct Magi80GraphicsReferenceScene scene;
    const uint8_t *read_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT];
    ULONG origin_x = (BENCH_SCREEN_WIDTH - profile->width) / 2U;
    ULONG origin_y = (BENCH_SCREEN_HEIGHT - profile->height) / 2U;
    size_t plane;

    unpack_source(source, layout, profile->width, profile->height,
                  oracle_front);
    memset(&scene, 0, sizeof(scene));
    scene.planar.surface.pixels = planar_source;
    scene.planar.surface.width = BENCH_SCREEN_WIDTH;
    scene.planar.surface.height = BENCH_SCREEN_HEIGHT;
    scene.planar.surface.stride = BENCH_SCREEN_WIDTH;
    scene.planar.width = BENCH_SCREEN_WIDTH;
    scene.planar.height = BENCH_SCREEN_HEIGHT;
    scene.planar.enabled = 1U;
    scene.pixel.surface.pixels = oracle_front;
    scene.pixel.surface.width = profile->width;
    scene.pixel.surface.height = profile->height;
    scene.pixel.surface.stride = profile->width;
    scene.pixel.width = profile->width;
    scene.pixel.height = profile->height;
    scene.pixel.screen_x = (int32_t)origin_x;
    scene.pixel.screen_y = (int32_t)origin_y;
    scene.pixel.enabled = 1U;

    if (magi80_graphics_reference_compose(
            &scene, canonical_expected, BENCH_SCREEN_WIDTH) !=
        MAGI80_GRAPHICS_REFERENCE_OK) {
        return 0;
    }
    for (plane = 0U; plane < MAGI80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        read_planes[plane] = physical_planes[plane];
    }
    if (magi80_aga_reference_decode_dual_playfield(
            read_planes, plane_stride, canonical_actual,
            BENCH_SCREEN_WIDTH) != MAGI80_AGA_REFERENCE_OK ||
        memcmp(canonical_expected, canonical_actual,
               BENCH_SCREEN_PIXELS) != 0) {
        return 0;
    }
    *oracle_checksum = fnv1a32(canonical_expected, BENCH_SCREEN_PIXELS);
    *canonical_checksum = fnv1a32(canonical_actual, BENCH_SCREEN_PIXELS);
    return *oracle_checksum == *canonical_checksum;
}

static int run_case(
    const struct ViewportProfile *profile,
    enum SourceLayout layout,
    enum CpuBackend backend,
    ULONG eclock_hz,
    uint8_t *planar_source,
    uint8_t *oracle_front,
    uint8_t *canonical_expected,
    uint8_t *canonical_actual,
    uint8_t *physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT],
    size_t plane_stride,
    struct BenchmarkResult *result)
{
    ULONG pixels = profile->width * profile->height;
    ULONG source_bytes =
        layout == SOURCE_PACKED4 ? pixels >> 1 : pixels;
    ULONG psets = pixels >> 2;
    ULONG origin_x = (BENCH_SCREEN_WIDTH - profile->width) / 2U;
    ULONG origin_y = (BENCH_SCREEN_HEIGHT - profile->height) / 2U;
    uint8_t *source = NULL;
    uint8_t *front_planes[MAGI80_C2P4_PLANE_COUNT];
    ULONG source_ticks[BENCH_SAMPLES];
    ULONG draw_ticks[BENCH_SAMPLES];
    ULONG conversion_ticks[BENCH_SAMPLES];
    ULONG blitter_ticks[BENCH_SAMPLES];
    ULONG blitter_wait_ticks[BENCH_SAMPLES];
    ULONG total_ticks[BENCH_SAMPLES];
    ULONG plane_bytes = (profile->width >> 3) * profile->height;
    ULONG lookup_traffic_bytes =
        backend == BACKEND_PAIR_LUT_C99 ||
                backend == BACKEND_PAIR_LUT_M68K ||
                backend == BACKEND_PAIR_LUT_M68K_BLIT
            ? pixels * 2U
            : 0U;
    ULONG scratch_bytes =
        backend == BACKEND_PAIR_LUT_M68K_BLIT
            ? plane_bytes * MAGI80_C2P4_PLANE_COUNT
            : 0U;
    size_t conversion_plane_stride =
        scratch_bytes != 0U ? (size_t)(profile->width >> 3) : plane_stride;
    uint8_t *scratch = NULL;
    struct BitMap source_bitmap;
    struct BitMap destination_bitmap;
    ULONG attempt;
    ULONG sample = 0U;
    size_t plane;
    int success = 0;

    source = (uint8_t *)AllocMem(source_bytes, MEMF_CHIP | MEMF_CLEAR);
    if (source == NULL || (TypeOfMem(source) & MEMF_CHIP) == 0U) {
        if (source != NULL) {
            FreeMem(source, source_bytes);
        }
        return 0;
    }
    if (scratch_bytes != 0U) {
        scratch =
            (uint8_t *)AllocMem(scratch_bytes, MEMF_CHIP | MEMF_CLEAR);
        if (scratch == NULL || (TypeOfMem(scratch) & MEMF_CHIP) == 0U) {
            goto cleanup;
        }
    }
    clear_front_planes(physical_planes, plane_stride);
    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        if (scratch != NULL) {
            front_planes[plane] = scratch + (plane * plane_bytes);
        } else {
            front_planes[plane] =
                physical_planes[plane << 1] +
                (origin_y * plane_stride) + (origin_x >> 3);
        }
    }
    if (scratch != NULL) {
        InitBitMap(&source_bitmap, MAGI80_C2P4_PLANE_COUNT,
                   (LONG)profile->width, (LONG)profile->height);
        InitBitMap(&destination_bitmap, MAGI80_C2P4_PLANE_COUNT,
                   BENCH_SCREEN_WIDTH, BENCH_SCREEN_HEIGHT);
        destination_bitmap.BytesPerRow = (UWORD)plane_stride;
        for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
            source_bitmap.Planes[plane] = (PLANEPTR)front_planes[plane];
            destination_bitmap.Planes[plane] =
                (PLANEPTR)physical_planes[plane << 1];
        }
    }

    for (attempt = 0U; attempt <= BENCH_SAMPLES; ++attempt) {
        struct EClockVal total_start;
        struct EClockVal total_end;
        struct EClockVal phase_start;
        struct EClockVal phase_end;
        ULONG measured_source;
        ULONG measured_draw;
        ULONG measured_conversion;
        ULONG measured_blitter = 0U;
        ULONG measured_blitter_wait = 0U;

        WaitTOF();
        (void)ReadEClock(&total_start);
        (void)ReadEClock(&phase_start);
        build_source(source, layout, profile->width, profile->height);
        (void)ReadEClock(&phase_end);
        measured_source = elapsed_ticks(&phase_start, &phase_end);

        (void)ReadEClock(&phase_start);
        draw_psets(source, layout, pixels, psets);
        (void)ReadEClock(&phase_end);
        measured_draw = elapsed_ticks(&phase_start, &phase_end);

        (void)ReadEClock(&phase_start);
        if (convert_source(source, layout, backend, profile->width,
                           profile->height, front_planes,
                           conversion_plane_stride) != MAGI80_C2P4_OK) {
            goto cleanup;
        }
        (void)ReadEClock(&phase_end);
        measured_conversion = elapsed_ticks(&phase_start, &phase_end);

        if (scratch != NULL) {
            struct EClockVal wait_start;
            struct EClockVal wait_end;

            (void)ReadEClock(&phase_start);
            if (BltBitMap(&source_bitmap, 0, 0, &destination_bitmap,
                          (LONG)origin_x, (LONG)origin_y,
                          (LONG)profile->width, (LONG)profile->height,
                          0xc0U, 0x0fU, NULL) !=
                (LONG)MAGI80_C2P4_PLANE_COUNT) {
                goto cleanup;
            }
            (void)ReadEClock(&wait_start);
            WaitBlit();
            (void)ReadEClock(&wait_end);
            (void)ReadEClock(&phase_end);
            measured_blitter = elapsed_ticks(&phase_start, &phase_end);
            measured_blitter_wait =
                elapsed_ticks(&wait_start, &wait_end);
        }
        (void)ReadEClock(&total_end);

        if (attempt != 0U) {
            source_ticks[sample] = measured_source;
            draw_ticks[sample] = measured_draw;
            conversion_ticks[sample] = measured_conversion;
            blitter_ticks[sample] = measured_blitter;
            blitter_wait_ticks[sample] = measured_blitter_wait;
            total_ticks[sample] = elapsed_ticks(&total_start, &total_end);
            ++sample;
        }
    }

    result->profile_name = profile->name;
    result->layout_name =
        layout == SOURCE_PACKED4 ? "packed4" : "byte4";
    if (backend == BACKEND_SCALAR_C99) {
        result->backend_name = "scalar_c99";
    } else if (backend == BACKEND_PAIR_LUT_C99) {
        result->backend_name = "pair_lut_c99";
    } else if (backend == BACKEND_PAIR_LUT_M68K) {
        result->backend_name = "pair_lut_m68k";
    } else if (backend == BACKEND_MASK32_M68K) {
        result->backend_name = "mask32_m68k";
    } else {
        result->backend_name = "pair_lut_m68k_blit";
    }
    result->blitter_role =
        scratch != NULL ? "staged_publish" : "none";
    result->publication_mode =
        scratch != NULL ? "blitter_copy" : "direct_active_screen";
    result->width = profile->width;
    result->height = profile->height;
    result->samples = BENCH_SAMPLES;
    result->memory_bytes = BENCH_DISPLAY_BYTES + source_bytes;
    if (backend == BACKEND_PAIR_LUT_C99 ||
        backend == BACKEND_PAIR_LUT_M68K ||
        backend == BACKEND_PAIR_LUT_M68K_BLIT) {
        result->memory_bytes += sizeof(pair_lut);
    }
    result->memory_bytes += scratch_bytes;
    result->dirty_bytes = pixels;
    result->dirty_regions = 1U;
    result->objects = 0U;
    result->fallback_objects = 0U;
    result->frame_budget_ticks = eclock_hz / 25U;
    result->source_median_ticks = median_ticks(source_ticks);
    result->draw_median_ticks = median_ticks(draw_ticks);
    result->cpu_conversion_median_ticks = median_ticks(conversion_ticks);
    result->blitter_median_ticks = median_ticks(blitter_ticks);
    result->blitter_wait_median_ticks =
        median_ticks(blitter_wait_ticks);
    result->publication_median_ticks = result->blitter_median_ticks;
    sort_ticks(total_ticks);
    result->total_min_ticks = total_ticks[0];
    result->total_median_ticks = total_ticks[BENCH_SAMPLES / 2U];
    result->total_max_ticks = total_ticks[BENCH_SAMPLES - 1U];
    result->deadline_misses = 0U;
    for (sample = 0U; sample < BENCH_SAMPLES; ++sample) {
        if (total_ticks[sample] > result->frame_budget_ticks) {
            ++result->deadline_misses;
        }
    }
    result->source_bytes = source_bytes;
    result->scratch_bytes = scratch_bytes;
    result->psets = psets;
    result->lookup_traffic_bytes = lookup_traffic_bytes;
    result->minimum_chip_traffic_bytes =
        source_bytes +
        (layout == SOURCE_PACKED4 ? psets * 2U : psets) +
        source_bytes + (plane_bytes * MAGI80_C2P4_PLANE_COUNT) +
        lookup_traffic_bytes +
        (scratch_bytes != 0U
             ? 2U * plane_bytes * MAGI80_C2P4_PLANE_COUNT
             : 0U);
    if (!verify_case(source, layout, profile, planar_source, oracle_front,
                     canonical_expected, canonical_actual, physical_planes,
                     plane_stride, &result->oracle_checksum,
                     &result->canonical_checksum)) {
        goto cleanup;
    }
    success = 1;

cleanup:
    if (scratch != NULL) {
        FreeMem(scratch, scratch_bytes);
    }
    FreeMem(source, source_bytes);
    return success;
}

static int write_case(BPTR output, const struct BenchmarkResult *result)
{
    return write_text(output, "case=") &&
           write_text(output, result->layout_name) &&
           write_text(output, "_") &&
           write_text(output, result->profile_name) &&
           write_text(output, "_") &&
           write_text(output, result->backend_name) &&
           write_text(output, " backend=") &&
           write_text(output, result->backend_name) &&
           write_text(output, " workload=full_rebuild_quarter_pset") &&
           write_key_decimal(output, " width=", result->width) &&
           write_key_decimal(output, " height=", result->height) &&
           write_key_decimal(output, " samples=", result->samples) &&
           write_key_decimal(output, " memory_bytes=", result->memory_bytes) &&
           write_key_decimal(output, " dirty_bytes=", result->dirty_bytes) &&
           write_key_decimal(output, " dirty_regions=", result->dirty_regions) &&
           write_key_decimal(output, " objects=", result->objects) &&
           write_key_decimal(output, " fallback_objects=",
                             result->fallback_objects) &&
           write_key_decimal(output, " frame_budget_ticks=",
                             result->frame_budget_ticks) &&
           write_key_decimal(output, " source_median_ticks=",
                             result->source_median_ticks) &&
           write_key_decimal(output, " draw_median_ticks=",
                             result->draw_median_ticks) &&
           write_key_decimal(output, " cpu_conversion_median_ticks=",
                             result->cpu_conversion_median_ticks) &&
           write_key_decimal(output, " blitter_median_ticks=",
                             result->blitter_median_ticks) &&
           write_key_decimal(output, " blitter_wait_median_ticks=",
                             result->blitter_wait_median_ticks) &&
           write_key_decimal(output, " publication_median_ticks=",
                             result->publication_median_ticks) &&
           write_key_decimal(output, " total_min_ticks=",
                             result->total_min_ticks) &&
           write_key_decimal(output, " total_median_ticks=",
                             result->total_median_ticks) &&
           write_key_decimal(output, " total_max_ticks=",
                             result->total_max_ticks) &&
           write_key_decimal(output, " oracle_checksum=",
                             result->oracle_checksum) &&
           write_key_decimal(output, " canonical_checksum=",
                             result->canonical_checksum) &&
           write_key_decimal(output, " deadline_misses=",
                             result->deadline_misses) &&
           write_text(output, " source_layout=") &&
           write_text(output, result->layout_name) &&
           write_key_decimal(output, " source_bytes=", result->source_bytes) &&
           write_key_decimal(output, " scratch_bytes=", result->scratch_bytes) &&
           write_key_decimal(output, " psets=", result->psets) &&
           write_key_decimal(output, " lookup_traffic_bytes=",
                             result->lookup_traffic_bytes) &&
           write_key_decimal(output, " minimum_chip_traffic_bytes=",
                             result->minimum_chip_traffic_bytes) &&
           write_key_decimal(output,
                             " display_plane_fetch_bytes_per_video_frame=",
                             BENCH_DISPLAY_BYTES) &&
           write_key_decimal(output, " video_hz=", 50U) &&
           write_text(output, " destination=pf1 blitter_role=") &&
           write_text(output, result->blitter_role) &&
           write_text(output, " publication_mode=") &&
           write_text(output, result->publication_mode) &&
           write_text(output,
                      " timing_scope=hosted_cooperative result=pass\n");
}

static int write_report_header(BPTR output, ULONG eclock_hz, ULONG overhead)
{
    if (!write_text(output,
                    "graphics_benchmark_format=1\n"
                    "benchmark=c2p4\n"
                    "environment=" MAGI80_BENCHMARK_ENVIRONMENT "\n"
                    "timing_authority="
                    MAGI80_BENCHMARK_TIMING_AUTHORITY "\n"
                    "timing_source=eclock\n"
                    "eclock_hz=") ||
        !write_decimal(output, eclock_hz) ||
        !write_text(output, "\ntimer_overhead_ticks=") ||
        !write_decimal(output, overhead) ||
        !write_text(output,
                    "\ncanonical_format=palette_identity_u8\n"
                    "checksum_algorithm=fnv1a32\n"
                    "display_dma=active\n"
                    "sprite_dma=active\n"
                    "audio_dma=inactive\n"
                    "case_count=") ||
        !write_decimal(output, BENCH_CASE_COUNT) ||
        !write_text(output, "\n")) {
        return 0;
    }
    return 1;
}

int main(void)
{
    static struct TagItem video_control[] = {
        {VTAG_PF1_BASE_SET, 0U},
        {VTAG_PF2_BASE_SET, 16U},
        {VTAG_FULLPALETTE_SET, TRUE},
        {TAG_DONE, 0U}
    };
    struct TagItem screen_tags[] = {
        {SA_DisplayID, BENCH_DISPLAY_ID},
        {SA_Width, BENCH_SCREEN_WIDTH},
        {SA_Height, BENCH_SCREEN_HEIGHT},
        {SA_Depth, MAGI80_AGA_REFERENCE_PLANE_COUNT},
        {SA_Type, CUSTOMSCREEN},
        {SA_Quiet, TRUE},
        {SA_ShowTitle, FALSE},
        {SA_Draggable, FALSE},
        {SA_Exclusive, TRUE},
        {SA_AutoScroll, FALSE},
        {SA_Interleaved, FALSE},
        {SA_ColorMapEntries, 32U},
        {SA_FullPalette, TRUE},
        {SA_VideoControl, (ULONG)(APTR)video_control},
        {TAG_DONE, 0U}
    };
    struct timerequest timer_request = {0};
    struct Screen *screen = NULL;
    struct BitMap *bitmap = NULL;
    uint8_t *physical_planes[MAGI80_AGA_REFERENCE_PLANE_COUNT];
    uint8_t *planar_source = NULL;
    uint8_t *oracle_front = NULL;
    uint8_t *canonical_expected = NULL;
    uint8_t *canonical_actual = NULL;
    /*
     * Stream one result at a time. Keeping the complete matrix here costs
     * roughly 3 KiB and can exhaust the small default AmigaDOS process stack
     * during cleanup, turning a completed matrix into an ambiguous timeout.
     */
    struct BenchmarkResult result;
    struct EClockVal clock_value;
    ULONG eclock_hz = 0U;
    ULONG overhead = 0U;
    ULONG chip_revision;
    size_t profile;
    size_t layout;
    size_t backend;
    size_t plane;
    int timer_open = 0;
    const char *failure = NULL;

    memset(&result, 0, sizeof(result));
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39U);
    IntuitionBase =
        (struct IntuitionBase *)OpenLibrary("intuition.library", 39U);
    if (GfxBase == NULL || IntuitionBase == NULL) {
        failure = "open_graphics_libraries_v39";
        goto cleanup;
    }
    chip_revision = GfxBase->ChipRevBits0;
    if ((chip_revision & (GFXF_AA_ALICE | GFXF_AA_LISA)) !=
        (GFXF_AA_ALICE | GFXF_AA_LISA)) {
        chip_revision = SetChipRev(SETCHIPREV_BEST);
    }
    if ((chip_revision & (GFXF_AA_ALICE | GFXF_AA_LISA)) !=
        (GFXF_AA_ALICE | GFXF_AA_LISA)) {
        failure = "aga_chipset";
        goto cleanup;
    }
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK,
                   (struct IORequest *)&timer_request, 0U) != 0) {
        failure = "open_timer_device";
        goto cleanup;
    }
    timer_open = 1;
    TimerBase = timer_request.tr_node.io_Device;
    eclock_hz = ReadEClock(&clock_value);
    overhead = timer_overhead();

    screen = OpenScreenTagList(NULL, screen_tags);
    if (screen == NULL) {
        failure = "open_aga_screen";
        goto cleanup;
    }
    bitmap = screen->RastPort.BitMap;
    if (bitmap == NULL || bitmap->BytesPerRow < BENCH_SCREEN_WIDTH / 8U ||
        bitmap->Depth != MAGI80_AGA_REFERENCE_PLANE_COUNT) {
        failure = "screen_bitmap_layout";
        goto cleanup;
    }
    for (plane = 0U; plane < MAGI80_AGA_REFERENCE_PLANE_COUNT; ++plane) {
        if (bitmap->Planes[plane] == NULL ||
            (TypeOfMem(bitmap->Planes[plane]) & MEMF_CHIP) == 0U) {
            failure = "screen_chip_planes";
            goto cleanup;
        }
        physical_planes[plane] = (uint8_t *)bitmap->Planes[plane];
    }

    planar_source =
        (uint8_t *)AllocMem(BENCH_SCREEN_PIXELS, MEMF_PUBLIC | MEMF_CLEAR);
    oracle_front =
        (uint8_t *)AllocMem(BENCH_SCREEN_PIXELS, MEMF_PUBLIC | MEMF_CLEAR);
    canonical_expected =
        (uint8_t *)AllocMem(BENCH_SCREEN_PIXELS, MEMF_PUBLIC | MEMF_CLEAR);
    canonical_actual =
        (uint8_t *)AllocMem(BENCH_SCREEN_PIXELS, MEMF_PUBLIC | MEMF_CLEAR);
    if (planar_source == NULL || oracle_front == NULL ||
        canonical_expected == NULL || canonical_actual == NULL) {
        failure = "allocate_validation_buffers";
        goto cleanup;
    }
    magi80_c2p4_build_pair_lut(pair_lut);
    if (!prepare_planar_base(planar_source, physical_planes,
                             (size_t)bitmap->BytesPerRow)) {
        failure = "prepare_planar_base";
        goto cleanup;
    }
    if (!write_report_header(Output(), eclock_hz, overhead)) {
        failure = "write_report_header";
        goto cleanup;
    }

    for (profile = 0U; profile < BENCH_PROFILE_COUNT; ++profile) {
        for (layout = 0U; layout < BENCH_LAYOUT_COUNT; ++layout) {
            for (backend = 0U; backend < BENCH_BACKEND_COUNT; ++backend) {
                if (!run_case(
                        &profiles[profile], (enum SourceLayout)layout,
                        benchmark_backends[backend], eclock_hz, planar_source,
                        oracle_front, canonical_expected, canonical_actual,
                        physical_planes, (size_t)bitmap->BytesPerRow,
                        &result)) {
                    failure = "benchmark_case";
                    goto cleanup;
                }
                if (!write_case(Output(), &result)) {
                    failure = "write_report_case";
                    goto cleanup;
                }
            }
        }
    }

cleanup:
    if (canonical_actual != NULL) {
        FreeMem(canonical_actual, BENCH_SCREEN_PIXELS);
    }
    if (canonical_expected != NULL) {
        FreeMem(canonical_expected, BENCH_SCREEN_PIXELS);
    }
    if (oracle_front != NULL) {
        FreeMem(oracle_front, BENCH_SCREEN_PIXELS);
    }
    if (planar_source != NULL) {
        FreeMem(planar_source, BENCH_SCREEN_PIXELS);
    }
    if (screen != NULL && !CloseScreen(screen) && failure == NULL) {
        failure = "close_screen";
    }
    if (timer_open) {
        CloseDevice((struct IORequest *)&timer_request);
        TimerBase = NULL;
    }
    if (IntuitionBase != NULL) {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }
    if (GfxBase != NULL) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }
    if (failure != NULL) {
        return report_failure(failure);
    }
    if (!write_text(Output(), "result=pass\n")) {
        return RETURN_ERROR;
    }
    return RETURN_OK;
}
