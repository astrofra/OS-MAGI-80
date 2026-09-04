#include <stddef.h>
#include <stdint.h>

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

#include "graphics/c2p_reference.h"

#define BENCH_WIDTH 256U
#define BENCH_HEIGHT 256U
#define BENCH_PIXELS (BENCH_WIDTH * BENCH_HEIGHT)
#define BENCH_PACKED_BYTES (BENCH_PIXELS / 2U)
#define BENCH_FRONT_PSETS 16384U
#define BENCH_DISPLAY_ID (PAL_MONITOR_ID | LORESDPF_KEY)

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Device *TimerBase = NULL;

struct BenchmarkResult {
    const char *name;
    ULONG source_bytes;
    ULONG build_ticks;
    ULONG front_pset_ticks;
    ULONG c2p_ticks;
    ULONG phase_sum_ticks;
    ULONG checksum;
};

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

static int report_failure(const char *stage)
{
    BPTR output = Output();

    if (!write_text(output, "failure=") || !write_text(output, stage) ||
        !write_text(output, "\nresult=fail\n")) {
        return RETURN_ERROR;
    }
    return RETURN_FAIL;
}

static uint8_t front_at(ULONG index)
{
    ULONG x = index & 0xffU;
    ULONG y = index >> 8;

    return (uint8_t)(((x >> 3) + (y >> 4) + (x ^ y)) & 0x0fU);
}

static uint8_t back_at(ULONG index)
{
    ULONG x = index & 0xffU;
    ULONG y = index >> 8;

    return (uint8_t)(((x >> 4) ^ (y >> 3) ^ (x >> 5)) & 0x0fU);
}

static void build_fb8(uint8_t *combined)
{
    ULONG index;

    for (index = 0U; index < BENCH_PIXELS; ++index) {
        combined[index] =
            (uint8_t)((front_at(index) << 4) | back_at(index));
    }
}

static void build_packed4_x2(uint8_t *front, uint8_t *back)
{
    ULONG pair;

    for (pair = 0U; pair < BENCH_PACKED_BYTES; ++pair) {
        ULONG even = pair << 1;
        ULONG odd = even + 1U;

        front[pair] =
            (uint8_t)((front_at(even) << 4) | front_at(odd));
        back[pair] = (uint8_t)((back_at(even) << 4) | back_at(odd));
    }
}

static void build_byte4_x2(uint8_t *front, uint8_t *back)
{
    ULONG index;

    for (index = 0U; index < BENCH_PIXELS; ++index) {
        front[index] = front_at(index);
        back[index] = back_at(index);
    }
}

static void build_front_byte4_back_packed4(uint8_t *front, uint8_t *back)
{
    ULONG pair;

    for (pair = 0U; pair < BENCH_PACKED_BYTES; ++pair) {
        ULONG even = pair << 1;
        ULONG odd = even + 1U;

        front[even] = front_at(even);
        front[odd] = front_at(odd);
        back[pair] = (uint8_t)((back_at(even) << 4) | back_at(odd));
    }
}

static uint16_t next_lfsr(uint16_t state)
{
    uint16_t feedback = (uint16_t)(0U - (state & 1U));

    return (uint16_t)((state >> 1) ^ (feedback & 0xb400U));
}

static uint8_t update_color(uint16_t state)
{
    return (uint8_t)(((state >> 4) ^ (state >> 12)) & 0x0fU);
}

static void front_psets_fb8(uint8_t *combined)
{
    uint16_t state = 0xace1U;
    ULONG count;

    for (count = 0U; count < BENCH_FRONT_PSETS; ++count) {
        uint8_t value;

        state = next_lfsr(state);
        value = combined[state];
        combined[state] =
            (uint8_t)((value & 0x0fU) | (update_color(state) << 4));
    }
}

static void front_psets_packed4(uint8_t *front)
{
    uint16_t state = 0xace1U;
    ULONG count;

    for (count = 0U; count < BENCH_FRONT_PSETS; ++count) {
        uint8_t *destination;
        uint8_t color;

        state = next_lfsr(state);
        destination = &front[state >> 1];
        color = update_color(state);
        if ((state & 1U) == 0U) {
            *destination =
                (uint8_t)((*destination & 0x0fU) | (color << 4));
        } else {
            *destination = (uint8_t)((*destination & 0xf0U) | color);
        }
    }
}

static void front_psets_byte4(uint8_t *front)
{
    uint16_t state = 0xace1U;
    ULONG count;

    for (count = 0U; count < BENCH_FRONT_PSETS; ++count) {
        state = next_lfsr(state);
        front[state] = update_color(state);
    }
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

static ULONG checksum_planes(uint8_t *planes[MIGA80_C2P_PLANE_COUNT],
                             size_t plane_stride)
{
    ULONG checksum = 0x6d616769UL;
    size_t plane;
    size_t y;
    size_t x;

    for (plane = 0U; plane < MIGA80_C2P_PLANE_COUNT; ++plane) {
        for (y = 0U; y < BENCH_HEIGHT; ++y) {
            const uint8_t *row = planes[plane] + (y * plane_stride);

            for (x = 0U; x < (BENCH_WIDTH / 8U); ++x) {
                checksum ^= row[x];
                checksum = (checksum << 5) | (checksum >> 27);
                checksum += 0x9e3779b9UL;
            }
        }
    }
    return checksum;
}

static ULONG source_bytes_for_layout(enum Miga80C2PLayout layout)
{
    switch (layout) {
    case MIGA80_C2P_LAYOUT_FB8:
        return BENCH_PIXELS;
    case MIGA80_C2P_LAYOUT_PACKED4_X2:
        return BENCH_PIXELS;
    case MIGA80_C2P_LAYOUT_BYTE4_X2:
        return BENCH_PIXELS * 2U;
    case MIGA80_C2P_LAYOUT_FRONT_BYTE4_BACK_PACKED4:
        return BENCH_PIXELS + BENCH_PACKED_BYTES;
    }
    return 0U;
}

static const char *name_for_layout(enum Miga80C2PLayout layout)
{
    switch (layout) {
    case MIGA80_C2P_LAYOUT_FB8:
        return "fb8";
    case MIGA80_C2P_LAYOUT_PACKED4_X2:
        return "packed4_x2";
    case MIGA80_C2P_LAYOUT_BYTE4_X2:
        return "byte4_x2";
    case MIGA80_C2P_LAYOUT_FRONT_BYTE4_BACK_PACKED4:
        return "front_byte4_back_packed4";
    }
    return "invalid";
}

static int run_layout(enum Miga80C2PLayout layout,
                      uint8_t *planes[MIGA80_C2P_PLANE_COUNT],
                      size_t plane_stride, struct BenchmarkResult *result)
{
    struct EClockVal start;
    struct EClockVal end;
    uint8_t *front_or_combined = NULL;
    uint8_t *back = NULL;
    ULONG first_size;
    ULONG second_size = 0U;
    enum Miga80C2PStatus c2p_status = MIGA80_C2P_INVALID_ARGUMENT;
    int success = 0;

    switch (layout) {
    case MIGA80_C2P_LAYOUT_FB8:
        first_size = BENCH_PIXELS;
        break;
    case MIGA80_C2P_LAYOUT_PACKED4_X2:
        first_size = BENCH_PACKED_BYTES;
        second_size = BENCH_PACKED_BYTES;
        break;
    case MIGA80_C2P_LAYOUT_BYTE4_X2:
        first_size = BENCH_PIXELS;
        second_size = BENCH_PIXELS;
        break;
    case MIGA80_C2P_LAYOUT_FRONT_BYTE4_BACK_PACKED4:
        first_size = BENCH_PIXELS;
        second_size = BENCH_PACKED_BYTES;
        break;
    default:
        return 0;
    }

    front_or_combined =
        (uint8_t *)AllocMem(first_size, MEMF_CHIP | MEMF_CLEAR);
    if (front_or_combined == NULL) {
        goto cleanup;
    }
    if (second_size != 0U) {
        back = (uint8_t *)AllocMem(second_size, MEMF_CHIP | MEMF_CLEAR);
        if (back == NULL) {
            goto cleanup;
        }
    }

    WaitTOF();
    (void)ReadEClock(&start);
    switch (layout) {
    case MIGA80_C2P_LAYOUT_FB8:
        build_fb8(front_or_combined);
        break;
    case MIGA80_C2P_LAYOUT_PACKED4_X2:
        build_packed4_x2(front_or_combined, back);
        break;
    case MIGA80_C2P_LAYOUT_BYTE4_X2:
        build_byte4_x2(front_or_combined, back);
        break;
    case MIGA80_C2P_LAYOUT_FRONT_BYTE4_BACK_PACKED4:
        build_front_byte4_back_packed4(front_or_combined, back);
        break;
    }
    (void)ReadEClock(&end);
    result->build_ticks = elapsed_ticks(&start, &end);

    WaitTOF();
    (void)ReadEClock(&start);
    if (layout == MIGA80_C2P_LAYOUT_FB8) {
        front_psets_fb8(front_or_combined);
    } else if (layout == MIGA80_C2P_LAYOUT_PACKED4_X2) {
        front_psets_packed4(front_or_combined);
    } else {
        front_psets_byte4(front_or_combined);
    }
    (void)ReadEClock(&end);
    result->front_pset_ticks = elapsed_ticks(&start, &end);

    WaitTOF();
    (void)ReadEClock(&start);
    switch (layout) {
    case MIGA80_C2P_LAYOUT_FB8:
        c2p_status = miga80_c2p_reference(
            front_or_combined, BENCH_WIDTH, BENCH_HEIGHT, BENCH_WIDTH,
            planes, plane_stride);
        break;
    case MIGA80_C2P_LAYOUT_PACKED4_X2:
        c2p_status = miga80_c2p_reference_packed4_x2(
            front_or_combined, back, BENCH_WIDTH, BENCH_HEIGHT,
            BENCH_WIDTH / 2U, BENCH_WIDTH / 2U, planes, plane_stride);
        break;
    case MIGA80_C2P_LAYOUT_BYTE4_X2:
        c2p_status = miga80_c2p_reference_byte4_x2(
            front_or_combined, back, BENCH_WIDTH, BENCH_HEIGHT, BENCH_WIDTH,
            BENCH_WIDTH, planes, plane_stride);
        break;
    case MIGA80_C2P_LAYOUT_FRONT_BYTE4_BACK_PACKED4:
        c2p_status = miga80_c2p_reference_front_byte4_back_packed4(
            front_or_combined, back, BENCH_WIDTH, BENCH_HEIGHT, BENCH_WIDTH,
            BENCH_WIDTH / 2U, planes, plane_stride);
        break;
    }
    (void)ReadEClock(&end);
    result->c2p_ticks = elapsed_ticks(&start, &end);
    if (c2p_status != MIGA80_C2P_OK) {
        goto cleanup;
    }

    result->name = name_for_layout(layout);
    result->source_bytes = source_bytes_for_layout(layout);
    result->phase_sum_ticks =
        result->build_ticks + result->front_pset_ticks + result->c2p_ticks;
    result->checksum = checksum_planes(planes, plane_stride);
    success = 1;

cleanup:
    if (back != NULL) {
        FreeMem(back, second_size);
    }
    if (front_or_combined != NULL) {
        FreeMem(front_or_combined, first_size);
    }
    return success;
}

static int write_result(BPTR output, const struct BenchmarkResult *result)
{
    return write_text(output, "layout=") && write_text(output, result->name) &&
           write_text(output, " source_bytes=") &&
           write_decimal(output, result->source_bytes) &&
           write_text(output, " build_ticks=") &&
           write_decimal(output, result->build_ticks) &&
           write_text(output, " front_pset_ticks=") &&
           write_decimal(output, result->front_pset_ticks) &&
           write_text(output, " c2p_ticks=") &&
           write_decimal(output, result->c2p_ticks) &&
           write_text(output, " phase_sum_ticks=") &&
           write_decimal(output, result->phase_sum_ticks) &&
           write_text(output, " checksum=") &&
           write_decimal(output, result->checksum) &&
           write_text(output, "\n");
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
        {SA_Width, BENCH_WIDTH},
        {SA_Height, BENCH_HEIGHT},
        {SA_Depth, MIGA80_C2P_PLANE_COUNT},
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
    struct BitMap *bitmap;
    uint8_t *planes[MIGA80_C2P_PLANE_COUNT];
    struct BenchmarkResult results[4];
    BPTR output = Output();
    ULONG chip_revision;
    ULONG eclock_hz;
    ULONG overhead;
    struct EClockVal clock_value;
    ULONG expected_checksum = 0U;
    size_t plane;
    size_t layout;
    const char *failure = NULL;
    int timer_open = 0;

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
    if (bitmap == NULL || bitmap->BytesPerRow < (BENCH_WIDTH / 8U) ||
        bitmap->Depth != MIGA80_C2P_PLANE_COUNT) {
        failure = "screen_bitmap_layout";
        goto cleanup;
    }
    for (plane = 0U; plane < MIGA80_C2P_PLANE_COUNT; ++plane) {
        if (bitmap->Planes[plane] == NULL ||
            (TypeOfMem(bitmap->Planes[plane]) & MEMF_CHIP) == 0U) {
            failure = "screen_chip_planes";
            goto cleanup;
        }
        planes[plane] = (uint8_t *)bitmap->Planes[plane];
    }

    for (layout = 0U; layout < 4U; ++layout) {
        if (!run_layout((enum Miga80C2PLayout)layout, planes,
                        (size_t)bitmap->BytesPerRow, &results[layout])) {
            failure = "layout_execution";
            goto cleanup;
        }
        if (layout == 0U) {
            expected_checksum = results[layout].checksum;
        } else if (results[layout].checksum != expected_checksum) {
            failure = "layout_checksum_mismatch";
            goto cleanup;
        }
    }

    if (!write_text(output,
                    "benchmark_format=1\n"
                    "backend=reference_c99\n"
                    "compiler_optimization=O2\n"
                    "timing_source=eclock\n"
                    "timing_context=hosted_cooperative_display_dma_active\n"
                    "source_memory=chip\n"
                    "destination_memory=screen_chip\n"
                    "width=256\n"
                    "height=256\n"
                    "front_psets=16384\n"
                    "samples=1\n"
                    "eclock_hz=") ||
        !write_decimal(output, eclock_hz) ||
        !write_text(output, "\ntimer_overhead_ticks=") ||
        !write_decimal(output, overhead) || !write_text(output, "\n")) {
        failure = "write_report_header";
        goto cleanup;
    }
    for (layout = 0U; layout < 4U; ++layout) {
        if (!write_result(output, &results[layout])) {
            failure = "write_report_result";
            goto cleanup;
        }
    }
cleanup:
    if (screen != NULL) {
        if (!CloseScreen(screen) && failure == NULL) {
            failure = "close_screen";
        }
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
