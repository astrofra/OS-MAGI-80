#include <stddef.h>

#include <dos/dos.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <graphics/displayinfo.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <graphics/rastport.h>
#include <graphics/videocontrol.h>
#include <graphics/view.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <utility/tagitem.h>

#define MAGI80_SCREEN_WIDTH 256U
#define MAGI80_SCREEN_HEIGHT 256U
#define MAGI80_SCREEN_DEPTH 8U
#define MAGI80_DISPLAY_ID (PAL_MONITOR_ID | LORESDPF_KEY)
#define MAGI80_FRONT_PLANE_MASK 0x55U
#define MAGI80_ALL_PLANE_MASK 0xffU
#define MAGI80_PALETTE_COLORS 32U
#define MAGI80_MIN_PLANE_BYTES \
    ((MAGI80_SCREEN_WIDTH * MAGI80_SCREEN_HEIGHT * MAGI80_SCREEN_DEPTH) / 8U)
#define MAGI80_VISIBLE_FRAMES 50U

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;

static ULONG palette[1U + (MAGI80_PALETTE_COLORS * 3U) + 1U];
static ULONG palette_readback[MAGI80_PALETTE_COLORS * 3U];

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

static int report_failure(const char *stage)
{
    BPTR output = Output();

    if (!write_text(output, "failure=") || !write_text(output, stage) ||
        !write_text(output, "\nresult=fail\n")) {
        return RETURN_ERROR;
    }
    return RETURN_FAIL;
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

static int report_memory_failure(ULONG before, ULONG during, ULONG after)
{
    BPTR output = Output();

    if (!write_text(output, "failure=screen_memory_release\nchip_before=") ||
        !write_decimal(output, before) || !write_text(output, "\nchip_during=") ||
        !write_decimal(output, during) || !write_text(output, "\nchip_after=") ||
        !write_decimal(output, after) || !write_text(output, "\nresult=fail\n")) {
        return RETURN_ERROR;
    }
    return RETURN_FAIL;
}

static int report_success(void)
{
    static const char result[] =
        "libraries_v39=pass\n"
        "aga_chipset=pass\n"
        "pal_mode=pass\n"
        "logical_size=256x256\n"
        "dual_playfield_4x4=pass\n"
        "palette_banks_0_16=pass\n"
        "displayable_chip_planes=pass\n"
        "raster_pattern=pass\n"
        "screen_restoration=pass\n"
        "result=pass\n";

    if (!write_bytes(Output(), result, sizeof(result) - 1U)) {
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

static ULONG expand_nibble(ULONG value)
{
    return (value & 0x0fU) * 0x11111111UL;
}

static void prepare_palette(void)
{
    ULONG index;

    palette[0] = MAGI80_PALETTE_COLORS << 16;
    for (index = 0U; index < MAGI80_PALETTE_COLORS; ++index) {
        palette[1U + (index * 3U)] = expand_nibble(index);
        palette[2U + (index * 3U)] = expand_nibble(index * 5U);
        palette[3U + (index * 3U)] =
            expand_nibble(15U - (index & 0x0fU));
    }
    palette[1U + (MAGI80_PALETTE_COLORS * 3U)] = 0U;
}

static int verify_palette(struct ViewPort *view_port)
{
    ULONG index;

    LoadRGB32(view_port, palette);
    GetRGB32(view_port->ColorMap, 0U, MAGI80_PALETTE_COLORS,
             palette_readback);

    for (index = 0U; index < MAGI80_PALETTE_COLORS; ++index) {
        if ((palette_readback[index * 3U] >> 28) != (index & 0x0fU) ||
            (palette_readback[1U + (index * 3U)] >> 28) !=
                ((index * 5U) & 0x0fU) ||
            (palette_readback[2U + (index * 3U)] >> 28) !=
                (15U - (index & 0x0fU))) {
            return 0;
        }
    }
    return 1;
}

static UBYTE spread_four_bits(UBYTE value)
{
    value &= 0x0fU;
    return (UBYTE)((value & 0x01U) | ((value & 0x02U) << 1) |
                   ((value & 0x04U) << 2) | ((value & 0x08U) << 3));
}

static UBYTE encode_playfields(UBYTE front, UBYTE back)
{
    return (UBYTE)(spread_four_bits(front) | (spread_four_bits(back) << 1));
}

static UBYTE background_at(ULONG x, ULONG y)
{
    return (UBYTE)(((x >> 4) ^ (y >> 4)) & 0x0fU);
}

static void draw_pattern(struct RastPort *rast_port)
{
    ULONG tile_x;
    ULONG tile_y;

    (void)SetWriteMask(rast_port, MAGI80_ALL_PLANE_MASK);
    for (tile_y = 0U; tile_y < 16U; ++tile_y) {
        for (tile_x = 0U; tile_x < 16U; ++tile_x) {
            SetAPen(rast_port,
                    (ULONG)encode_playfields(
                        0U, (UBYTE)((tile_x ^ tile_y) & 0x0fU)));
            RectFill(rast_port, (LONG)(tile_x * 16U), (LONG)(tile_y * 16U),
                     (LONG)((tile_x * 16U) + 15U),
                     (LONG)((tile_y * 16U) + 15U));
        }
    }

    (void)SetWriteMask(rast_port, MAGI80_FRONT_PLANE_MASK);
    SetAPen(rast_port, (ULONG)encode_playfields(1U, 0U));
    RectFill(rast_port, 48L, 48L, 207L, 79L);
    SetAPen(rast_port, (ULONG)encode_playfields(15U, 0U));
    RectFill(rast_port, 96L, 96L, 159L, 159L);
    (void)SetWriteMask(rast_port, MAGI80_ALL_PLANE_MASK);
    WaitBlit();
}

static int verify_pixel(struct RastPort *rast_port, ULONG x, ULONG y,
                        UBYTE front)
{
    ULONG pixel = ReadPixel(rast_port, (LONG)x, (LONG)y);
    UBYTE expected = encode_playfields(front, background_at(x, y));

    return pixel != 0xffffffffUL && (UBYTE)pixel == expected;
}

static int verify_pattern(struct RastPort *rast_port)
{
    return verify_pixel(rast_port, 8U, 24U, 0U) &&
           verify_pixel(rast_port, 52U, 52U, 1U) &&
           verify_pixel(rast_port, 180U, 60U, 1U) &&
           verify_pixel(rast_port, 100U, 100U, 15U) &&
           verify_pixel(rast_port, 220U, 220U, 0U);
}

static int verify_bitmap(const struct Screen *screen)
{
    const struct BitMap *bitmap = screen->RastPort.BitMap;
    ULONG plane;

    if (bitmap == NULL ||
        GetBitMapAttr(bitmap, BMA_WIDTH) < MAGI80_SCREEN_WIDTH ||
        GetBitMapAttr(bitmap, BMA_HEIGHT) < MAGI80_SCREEN_HEIGHT ||
        GetBitMapAttr(bitmap, BMA_DEPTH) != MAGI80_SCREEN_DEPTH ||
        (GetBitMapAttr(bitmap, BMA_FLAGS) &
         (BMF_DISPLAYABLE | BMF_STANDARD)) !=
            (BMF_DISPLAYABLE | BMF_STANDARD)) {
        return 0;
    }
    for (plane = 0U; plane < MAGI80_SCREEN_DEPTH; ++plane) {
        if (bitmap->Planes[plane] == NULL ||
            (TypeOfMem(bitmap->Planes[plane]) & MEMF_CHIP) == 0U) {
            return 0;
        }
    }
    return 1;
}

static int verify_palette_bases(struct ColorMap *color_map)
{
    struct TagItem query[] = {
        {VTAG_PF1_BASE_GET, 0U},
        {VTAG_PF2_BASE_GET, 0U},
        {TAG_DONE, 0U}
    };

    if (VideoControl(color_map, query) != 0U) {
        return 0;
    }
    return query[0].ti_Tag == VTAG_PF1_BASE_SET &&
           query[0].ti_Data == 0U &&
           query[1].ti_Tag == VTAG_PF2_BASE_SET &&
           query[1].ti_Data == 16U;
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
        {SA_DisplayID, MAGI80_DISPLAY_ID},
        {SA_Width, MAGI80_SCREEN_WIDTH},
        {SA_Height, MAGI80_SCREEN_HEIGHT},
        {SA_Depth, MAGI80_SCREEN_DEPTH},
        {SA_Type, CUSTOMSCREEN},
        {SA_Quiet, TRUE},
        {SA_ShowTitle, FALSE},
        {SA_Draggable, FALSE},
        {SA_Exclusive, TRUE},
        {SA_AutoScroll, FALSE},
        {SA_Interleaved, FALSE},
        {SA_ColorMapEntries, MAGI80_PALETTE_COLORS},
        {SA_FullPalette, TRUE},
        {SA_VideoControl, (ULONG)(APTR)video_control},
        {TAG_DONE, 0U}
    };
    struct DisplayInfo display_info = {0};
    DisplayInfoHandle display_handle;
    struct Screen *screen = NULL;
    const char *failure = NULL;
    ULONG chip_before = 0U;
    ULONG chip_during = 0U;
    ULONG chip_after;
    ULONG chip_second_during = 0U;
    ULONG chip_final = 0U;
    ULONG chip_revision;
    ULONG frame;
    BOOL completed = FALSE;
    BOOL memory_release_failure = FALSE;

    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39U);
    if (GfxBase == NULL) {
        return report_failure("open_graphics_v39");
    }
    IntuitionBase =
        (struct IntuitionBase *)OpenLibrary("intuition.library", 39U);
    if (IntuitionBase == NULL) {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
        return report_failure("open_intuition_v39");
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

    display_handle = FindDisplayInfo(MAGI80_DISPLAY_ID);
    if (display_handle == NULL ||
        GetDisplayInfoData(display_handle, &display_info,
                           (ULONG)sizeof(display_info), DTAG_DISP,
                           MAGI80_DISPLAY_ID) == 0U ||
        display_info.NotAvailable != 0U ||
        (display_info.PropertyFlags &
         (DIPF_IS_PAL | DIPF_IS_DUALPF)) !=
            (DIPF_IS_PAL | DIPF_IS_DUALPF) ||
        (display_info.PropertyFlags & DIPF_IS_PF2PRI) != 0U ||
        ModeNotAvailable(MAGI80_DISPLAY_ID) != 0L) {
        failure = "pal_aga_dualpf_mode";
        goto cleanup;
    }

    chip_before = AvailMem(MEMF_CHIP);
    screen = OpenScreenTagList(NULL, screen_tags);
    if (screen == NULL) {
        failure = "open_256x256x8_screen";
        goto cleanup;
    }
    chip_during = AvailMem(MEMF_CHIP);

    if (screen->Width != (WORD)MAGI80_SCREEN_WIDTH ||
        screen->Height != (WORD)MAGI80_SCREEN_HEIGHT) {
        failure = "logical_screen_size";
        goto cleanup;
    }
    if ((ULONG)GetVPModeID(&screen->ViewPort) != MAGI80_DISPLAY_ID ||
        (screen->ViewPort.Modes & DUALPF) == 0U ||
        (screen->ViewPort.Modes & PFBA) != 0U ||
        screen->ViewPort.RasInfo == NULL ||
        screen->ViewPort.RasInfo->Next == NULL) {
        failure = "dual_playfield_layout";
        goto cleanup;
    }
    if (!verify_bitmap(screen)) {
        failure = "displayable_chip_planes";
        goto cleanup;
    }
    if (chip_before <= chip_during ||
        chip_before - chip_during < MAGI80_MIN_PLANE_BYTES) {
        failure = "chip_allocation_delta";
        goto cleanup;
    }
    if (screen->ViewPort.ColorMap == NULL ||
        screen->ViewPort.ColorMap->Count < MAGI80_PALETTE_COLORS ||
        !verify_palette_bases(screen->ViewPort.ColorMap)) {
        failure = "palette_banks";
        goto cleanup;
    }

    prepare_palette();
    if (!verify_palette(&screen->ViewPort)) {
        failure = "palette_roundtrip";
        goto cleanup;
    }
    draw_pattern(&screen->RastPort);
    if (!verify_pattern(&screen->RastPort)) {
        failure = "raster_pattern";
        goto cleanup;
    }
    for (frame = 0U; frame < MAGI80_VISIBLE_FRAMES; ++frame) {
        WaitTOF();
    }
    completed = TRUE;

cleanup:
    if (screen != NULL) {
        if (!CloseScreen(screen)) {
            if (failure == NULL) {
                failure = "close_screen";
            }
        } else {
            screen = NULL;
        }
    }
    WaitTOF();
    WaitTOF();
    chip_after = AvailMem(MEMF_CHIP);

    /* The first Intuition screen may populate persistent system caches.  A
       second identical cycle detects per-screen leakage without treating
       those one-time allocations as a MAGI-80 leak. */
    if (completed && failure == NULL) {
        screen = OpenScreenTagList(NULL, screen_tags);
        if (screen == NULL) {
            failure = "repeat_open_screen";
        } else {
            chip_second_during = AvailMem(MEMF_CHIP);
            if (chip_after <= chip_second_during ||
                chip_after - chip_second_during < MAGI80_MIN_PLANE_BYTES) {
                failure = "repeat_chip_allocation_delta";
            }
            WaitTOF();
            WaitTOF();
            if (!CloseScreen(screen)) {
                if (failure == NULL) {
                    failure = "repeat_close_screen";
                }
            } else {
                screen = NULL;
            }
            WaitTOF();
            WaitTOF();
            chip_final = AvailMem(MEMF_CHIP);
            if (chip_final < chip_after && failure == NULL) {
                failure = "screen_memory_release";
                memory_release_failure = TRUE;
            }
        }
    }

    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase = NULL;
    CloseLibrary((struct Library *)GfxBase);
    GfxBase = NULL;

    if (failure != NULL) {
        if (memory_release_failure) {
            return report_memory_failure(chip_after, chip_second_during,
                                         chip_final);
        }
        return report_failure(failure);
    }
    return report_success();
}
