#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <devices/audio.h>
#include <devices/timer.h>
#include <dos/dos.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <exec/tasks.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <graphics/videocontrol.h>
#include <hardware/blit.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/timer.h>
#include <utility/tagitem.h>

#include "graphics/c2p4_reference.h"
#include "../chipram/kernels.h"

#define BENCH_SCREEN_WIDTH 256U
#define BENCH_SCREEN_HEIGHT 256U
#define BENCH_SCREEN_DEPTH 8U
#define BENCH_SCREEN_PLANE_BYTES \
    ((BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT) / 8U)
#define BENCH_DISPLAY_BYTES \
    (BENCH_SCREEN_PLANE_BYTES * BENCH_SCREEN_DEPTH)
#define BENCH_DISPLAY_ID (PAL_MONITOR_ID | LORESDPF_KEY)

#define BENCH_RAW_BUFFER_BYTES 8192U
#define BENCH_RAW_ITERATIONS 4U
#define BENCH_RAW_SAMPLES 7U
#define BENCH_RAW_KERNEL_COUNT 6U

#define BENCH_C2P_ITERATIONS 1U
#define BENCH_C2P_SAMPLES 3U
#define BENCH_C2P_PROFILE_COUNT 3U
#define BENCH_C2P_LAYOUT_COUNT 2U
#define BENCH_C2P_BACKEND_COUNT 2U

#define BENCH_DMA_PROFILE_COUNT 7U
#define BENCH_RAW_RESULT_COUNT \
    (BENCH_DMA_PROFILE_COUNT * BENCH_RAW_KERNEL_COUNT)
#define BENCH_C2P_RESULT_COUNT \
    (BENCH_DMA_PROFILE_COUNT * BENCH_C2P_PROFILE_COUNT * \
     BENCH_C2P_LAYOUT_COUNT * BENCH_C2P_BACKEND_COUNT)
#define BENCH_CASE_COUNT \
    (BENCH_RAW_RESULT_COUNT + BENCH_C2P_RESULT_COUNT)

#define BENCH_STACK_BYTES 16384U
#define BENCH_STACK_BOUNDARY_RESERVE_BYTES 256U
#define BENCH_STACK_GUARD_BYTES 256U
#define BENCH_STACK_TOTAL_BYTES \
    (BENCH_STACK_BYTES + (2U * BENCH_STACK_BOUNDARY_RESERVE_BYTES) + \
     (2U * BENCH_STACK_GUARD_BYTES))
#define BENCH_STACK_FILL 0xcdU
#define BENCH_STACK_LOWER_GUARD 0x5aU
#define BENCH_STACK_UPPER_GUARD 0xa5U

#define BENCH_AUDIO_CHANNELS 4U
#define BENCH_AUDIO_SAMPLE_BYTES 16384U
#define BENCH_AUDIO_PERIOD 124U
#define BENCH_BLITTER_BYTES 32768U
#define BENCH_BLITTER_WORDS_PER_ROW 64U
#define BENCH_BLITTER_ROWS 256U
#define BENCH_BLITTER_POLL_LIMIT 4000000UL
#define BENCH_RASTER_START_LINE 32U
#define BENCH_RASTER_POLL_LIMIT 4000000UL
#define BENCH_TIMER_RETRY_LIMIT 8U

#define BENCH_ALL_CUSTOM_INTERRUPTS 0x7fffU
#define BENCH_MANAGED_DMA \
    (DMAF_AUDIO | DMAF_SPRITE | DMAF_BLITTER | DMAF_COPPER | \
     DMAF_RASTER | DMAF_BLITHOG)

#ifndef MAGI80_BENCHMARK_ENVIRONMENT
#define MAGI80_BENCHMARK_ENVIRONMENT "fs_uae_a1200_pal"
#endif

#ifndef MAGI80_BENCHMARK_AUTHORITY
#define MAGI80_BENCHMARK_AUTHORITY "protocol_only"
#endif

#ifndef MAGI80_BENCHMARK_REPORT_PATH
#define MAGI80_BENCHMARK_REPORT_PATH NULL
#endif

struct GfxBase *GfxBase = NULL;
struct IntuitionBase *IntuitionBase = NULL;
struct Device *TimerBase = NULL;

enum RawOperation {
    RAW_WRITE_BYTE = 0,
    RAW_WRITE_WORD,
    RAW_WRITE_LONG,
    RAW_WRITE_LONG4,
    RAW_COPY_LONG4,
    RAW_READ_LONG4
};

enum C2PLayout {
    C2P_LAYOUT_PACKED4 = 0,
    C2P_LAYOUT_BYTE4
};

enum C2PBackend {
    C2P_BACKEND_PAIR_LUT_M68K = 0,
    C2P_BACKEND_MASK32_M68K
};

enum ProgressPhase {
    PHASE_HOSTED_PREPARE = 1,
    PHASE_STACK_ENTER = 2,
    PHASE_EXCLUSIVE_ENTER = 3,
    PHASE_EXCLUSIVE_CASE = 4,
    PHASE_EXCLUSIVE_LEAVE = 5,
    PHASE_STACK_LEAVE = 6,
    PHASE_VALIDATE = 7,
    PHASE_CLEANUP = 8,
    PHASE_COMPLETE = 9
};

struct DmaProfile {
    const char *name;
    UWORD fixed_bits;
    UBYTE use_audio;
    UBYTE use_blitter;
    UBYTE blitter_hog;
};

struct RawKernel {
    const char *name;
    const char *operation_name;
    const char *access_width;
    enum RawOperation operation;
    Magi80ChipRamKernel kernel;
    ULONG seed;
    ULONG traffic_multiplier;
};

struct C2PProfile {
    const char *name;
    ULONG width;
    ULONG height;
};

struct RawResult {
    const struct DmaProfile *dma;
    const struct RawKernel *kernel;
    ULONG minimum_ticks;
    ULONG median_ticks;
    ULONG maximum_ticks;
    ULONG deadline_misses;
    ULONG expected_checksum;
    ULONG actual_checksum;
    ULONG kernel_return_checksum;
};

struct C2PResult {
    const struct DmaProfile *dma;
    const struct C2PProfile *profile;
    enum C2PLayout layout;
    enum C2PBackend backend;
    ULONG minimum_ticks;
    ULONG median_ticks;
    ULONG maximum_ticks;
    ULONG deadline_misses;
    ULONG expected_checksum;
    ULONG actual_checksum;
    ULONG source_bytes;
    ULONG plane_write_bytes;
    ULONG lookup_traffic_bytes;
    ULONG minimum_chip_traffic_bytes;
};

struct BatchState {
    UWORD interrupt_enable;
    UWORD dma_enable;
};

static const struct DmaProfile dma_profiles[BENCH_DMA_PROFILE_COUNT] = {
    {"blanked", 0U, 0U, 0U, 0U},
    {"display", DMAF_RASTER, 0U, 0U, 0U},
    {"display_copper", DMAF_RASTER | DMAF_COPPER, 0U, 0U, 0U},
    {"display_copper_sprite",
     DMAF_RASTER | DMAF_COPPER | DMAF_SPRITE, 0U, 0U, 0U},
    {"display_copper_sprite_audio",
     DMAF_RASTER | DMAF_COPPER | DMAF_SPRITE, 1U, 0U, 0U},
    {"display_copper_sprite_audio_blitter_fair",
     DMAF_RASTER | DMAF_COPPER | DMAF_SPRITE, 1U, 1U, 0U},
    {"display_copper_sprite_audio_blitter_hog",
     DMAF_RASTER | DMAF_COPPER | DMAF_SPRITE, 1U, 1U, 1U}
};

static const struct RawKernel raw_kernels[BENCH_RAW_KERNEL_COUNT] = {
    {"write_byte", "write", "byte", RAW_WRITE_BYTE,
     magi80_chipram_write_byte, 0x00000012U, 1U},
    {"write_word", "write", "word", RAW_WRITE_WORD,
     magi80_chipram_write_word, 0x00003456U, 1U},
    {"write_long", "write", "long", RAW_WRITE_LONG,
     magi80_chipram_write_long, 0x89abcdefU, 1U},
    {"write_long4", "write", "long_unrolled4", RAW_WRITE_LONG4,
     magi80_chipram_write_long4, 0x13579bdfU, 1U},
    {"copy_long4", "copy", "long_unrolled4", RAW_COPY_LONG4,
     magi80_chipram_copy_long4, 0U, 2U},
    {"read_long4", "read", "long_unrolled4", RAW_READ_LONG4,
     magi80_chipram_read_long4, 0U, 1U}
};

static const struct C2PProfile c2p_profiles[BENCH_C2P_PROFILE_COUNT] = {
    {"160x128", 160U, 128U},
    {"192x160", 192U, 160U},
    {"256x256", 256U, 256U}
};

static struct timerequest timer_request;
static struct StackSwapStruct stack_swap;
static struct RawResult raw_results[BENCH_RAW_RESULT_COUNT];
static struct C2PResult c2p_results[BENCH_C2P_RESULT_COUNT];
static ULONG c2p_expected_checksum[BENCH_C2P_PROFILE_COUNT]
                                   [BENCH_C2P_LAYOUT_COUNT];
static struct IOAudio *audio_control;
static struct IOAudio audio_play[BENCH_AUDIO_CHANNELS];
static struct MsgPort *audio_port;
static UBYTE audio_allocation_map[] = {0x0fU};
static UBYTE audio_play_started[BENCH_AUDIO_CHANNELS];

static UBYTE *raw_source;
static UBYTE *raw_destination;
static UBYTE *packed_source;
static UBYTE *byte_source;
static uint32_t *pair_lut;
static UBYTE *oracle_planes_allocation;
static UBYTE *audio_sample;
static UBYTE *blitter_source;
static UBYTE *blitter_destination;
static UBYTE *stack_allocation;
static UBYTE *physical_planes[BENCH_SCREEN_DEPTH];

static volatile ULONG kernel_sink;
static volatile ULONG progress_phase;
static volatile ULONG progress_case;
static ULONG eclock_hz;
static ULONG timer_overhead_ticks;
static ULONG timer_discarded_samples;
static ULONG frame_budget_ticks;
static ULONG initial_stack_bytes;
static ULONG available_chip_bytes_before_setup;
static ULONG available_fast_bytes_before_setup;
static UWORD exec_version;
static UWORD exec_revision;
static UWORD attention_flags;
static UBYTE power_supply_hz;
static UBYTE detected_stock_constraints;
static ULONG initial_cache_bits;
static int cache_controlled;
static UWORD suite_initial_intena;
static UWORD suite_initial_dma;
static UWORD audio_dma_mask;
static ULONG expected_blitter_checksum;
static ULONG stack_high_water_bytes;
static ULONG dedicated_stack_pointer_offset;
static ULONG stack_inspection_code;
static ULONG stack_guard_mismatch_index;
static ULONG stack_guard_mismatch_value;
static ULONG blitter_timeout_count;
static ULONG raster_timeout_count;
static int suite_status;

static volatile struct Custom *const custom =
    (volatile struct Custom *)(uintptr_t)0x00dff000UL;

static ULONG elapsed_ticks(const struct EClockVal *start,
                           const struct EClockVal *end)
{
    return end->ev_lo - start->ev_lo;
}

static int measured_elapsed_ticks(const struct EClockVal *start,
                                  const struct EClockVal *end,
                                  ULONG *ticks)
{
    ULONG high_delta = end->ev_hi - start->ev_hi;
    ULONG elapsed;

    if (high_delta == 0U && end->ev_lo >= start->ev_lo) {
        elapsed = end->ev_lo - start->ev_lo;
    } else if (high_delta == 1U && end->ev_lo < start->ev_lo) {
        elapsed = (0xffffffffUL - start->ev_lo) + 1U + end->ev_lo;
    } else {
        return 0;
    }
    /* Every batch is designed to finish well below two PAL seconds. */
    if (elapsed > eclock_hz * 2U) {
        return 0;
    }
    *ticks = elapsed;
    return 1;
}

static ULONG measure_timer_overhead(void)
{
    struct EClockVal start;
    struct EClockVal end;
    ULONG minimum = 0xffffffffUL;
    size_t attempt;

    for (attempt = 0U; attempt < 32U; ++attempt) {
        (void)ReadEClock(&start);
        (void)ReadEClock(&end);
        if (elapsed_ticks(&start, &end) < minimum) {
            minimum = elapsed_ticks(&start, &end);
        }
    }
    return minimum;
}

static void sort_ticks(ULONG *values, size_t count)
{
    size_t outer;

    for (outer = 1U; outer < count; ++outer) {
        ULONG value = values[outer];
        size_t inner = outer;

        while (inner > 0U && values[inner - 1U] > value) {
            values[inner] = values[inner - 1U];
            --inner;
        }
        values[inner] = value;
    }
}

static ULONG fnv1a32_update(ULONG hash, const UBYTE *bytes, size_t length)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        hash ^= (ULONG)bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

static ULONG fnv1a32(const UBYTE *bytes, size_t length)
{
    return fnv1a32_update(2166136261UL, bytes, length);
}

static ULONG hash_front_planes(UBYTE *const planes[BENCH_SCREEN_DEPTH])
{
    ULONG hash = 2166136261UL;
    size_t logical_plane;

    for (logical_plane = 0U; logical_plane < MAGI80_C2P4_PLANE_COUNT;
         ++logical_plane) {
        hash = fnv1a32_update(hash, planes[logical_plane << 1],
                              BENCH_SCREEN_PLANE_BYTES);
    }
    return hash;
}

static ULONG hash_contiguous_c2p_planes(const UBYTE *allocation)
{
    return fnv1a32(allocation,
                   BENCH_SCREEN_PLANE_BYTES * MAGI80_C2P4_PLANE_COUNT);
}

static int check_region(const UBYTE *region, size_t length, UBYTE value)
{
    size_t index;

    for (index = 0U; index < length; ++index) {
        if (region[index] != value) {
            stack_guard_mismatch_index = (ULONG)index;
            stack_guard_mismatch_value = (ULONG)region[index];
            return 0;
        }
    }
    return 1;
}

static int stack_guards_intact(void)
{
    const UBYTE *usable =
        stack_allocation + BENCH_STACK_GUARD_BYTES +
        BENCH_STACK_BOUNDARY_RESERVE_BYTES;

    if (!check_region(stack_allocation, BENCH_STACK_GUARD_BYTES,
                      BENCH_STACK_LOWER_GUARD)) {
        stack_inspection_code = 1U;
        return 0;
    }
    if (!check_region(usable + BENCH_STACK_BYTES +
                          BENCH_STACK_BOUNDARY_RESERVE_BYTES,
                      BENCH_STACK_GUARD_BYTES,
                      BENCH_STACK_UPPER_GUARD)) {
        stack_inspection_code = 2U;
        return 0;
    }
    return 1;
}

static int inspect_stack(void)
{
    const UBYTE *usable =
        stack_allocation + BENCH_STACK_GUARD_BYTES +
        BENCH_STACK_BOUNDARY_RESERVE_BYTES;
    size_t untouched = 0U;

    stack_inspection_code = 0U;
    stack_guard_mismatch_index = 0U;
    stack_guard_mismatch_value = 0U;
    if (!stack_guards_intact()) {
        return 0;
    }
    if (dedicated_stack_pointer_offset >= BENCH_STACK_BYTES) {
        stack_inspection_code = 3U;
        return 0;
    }
    while (untouched < BENCH_STACK_BYTES &&
           usable[untouched] == BENCH_STACK_FILL) {
        ++untouched;
    }
    stack_high_water_bytes = BENCH_STACK_BYTES - (ULONG)untouched;
    if (stack_high_water_bytes == 0U) {
        stack_inspection_code = 4U;
        return 0;
    }
    if (stack_high_water_bytes >= BENCH_STACK_BYTES) {
        stack_inspection_code = 5U;
        return 0;
    }
    return 1;
}

static UWORD current_beam_line(void)
{
    UWORD high_before;
    UWORD high_after;
    UWORD low;

    /*
     * VPOSR and VHPOSR are separate 16-bit custom registers.  A 32-bit read
     * is not atomic on the custom-chip bus and can straddle line 255/256.
     * Retry that boundary so the returned 9-bit PAL line is coherent.
     */
    do {
        high_before = custom->vposr;
        low = custom->vhposr;
        high_after = custom->vposr;
    } while (((high_before ^ high_after) & 1U) != 0U);

    return (UWORD)(((high_after & 1U) << 8) | (low >> 8));
}

static int wait_for_raster_start(void)
{
    ULONG polls = 0U;

    while (current_beam_line() >= BENCH_RASTER_START_LINE) {
        if (++polls >= BENCH_RASTER_POLL_LIMIT) {
            ++raster_timeout_count;
            return 0;
        }
    }
    while (current_beam_line() < BENCH_RASTER_START_LINE) {
        if (++polls >= BENCH_RASTER_POLL_LIMIT) {
            ++raster_timeout_count;
            return 0;
        }
    }
    return 1;
}

static UWORD profile_dma_bits(const struct DmaProfile *profile)
{
    UWORD bits = profile->fixed_bits;

    if (profile->use_audio != 0U) {
        bits = (UWORD)(bits | audio_dma_mask);
    }
    if (profile->use_blitter != 0U) {
        bits = (UWORD)(bits | DMAF_BLITTER);
    }
    if (profile->blitter_hog != 0U) {
        bits = (UWORD)(bits | DMAF_BLITHOG);
    }
    return bits;
}

static void launch_blitter_copy(void)
{
    custom->bltcon0 = (UWORD)(BC0F_SRCA | BC0F_DEST | A_TO_D);
    custom->bltcon1 = 0U;
    custom->bltafwm = 0xffffU;
    custom->bltalwm = 0xffffU;
    custom->bltamod = 0U;
    custom->bltdmod = 0U;
    custom->bltapt = (APTR)blitter_source;
    custom->bltdpt = (APTR)blitter_destination;
    custom->bltsize =
        (UWORD)((BENCH_BLITTER_ROWS << HSIZEBITS) |
                (BENCH_BLITTER_WORDS_PER_ROW & HSIZEMASK));
}

static int wait_for_blitter(void)
{
    ULONG polls = 0U;

    /*
     * The NDK keeps the historical DMAF_BLTDONE name for DMACONR bit 14,
     * but the read-side signal is BBUSY: one means that the blitter is busy.
     * Read once before testing, as graphics.library/WaitBlit() does for the
     * early-Agnus start/final-word workaround (harmless on AGA).
     */
    (void)custom->dmaconr;
    while ((custom->dmaconr & DMAF_BLTDONE) != 0U) {
        ++polls;
        if (polls >= BENCH_BLITTER_POLL_LIMIT) {
            ++blitter_timeout_count;
            return 0;
        }
    }
    (void)custom->dmaconr;
    return 1;
}

static int restore_batch_state(const struct BatchState *state)
{
    UWORD restored_dma;
    UWORD restored_intena;

    Disable();
    custom->dmacon = BENCH_MANAGED_DMA;
    custom->dmacon =
        (UWORD)(DMAF_SETCLR | (state->dma_enable & BENCH_MANAGED_DMA));
    custom->intena = BENCH_ALL_CUSTOM_INTERRUPTS;
    custom->intena =
        (UWORD)(INTF_SETCLR |
                (state->interrupt_enable & BENCH_ALL_CUSTOM_INTERRUPTS));
    restored_dma = (UWORD)(custom->dmaconr & BENCH_MANAGED_DMA);
    restored_intena =
        (UWORD)(custom->intenar & BENCH_ALL_CUSTOM_INTERRUPTS);
    Enable();

    return restored_dma == (state->dma_enable & BENCH_MANAGED_DMA) &&
           restored_intena ==
               (state->interrupt_enable & BENCH_ALL_CUSTOM_INTERRUPTS);
}

static int begin_batch(const struct DmaProfile *profile,
                       struct BatchState *state)
{
    UWORD wanted = profile_dma_bits(profile);
    UWORD active;

    if (!wait_for_raster_start()) {
        return 0;
    }
    Disable();
    state->interrupt_enable = custom->intenar;
    state->dma_enable = custom->dmaconr;
    custom->intena = BENCH_ALL_CUSTOM_INTERRUPTS;
    custom->dmacon = BENCH_MANAGED_DMA;
    custom->dmacon = (UWORD)(DMAF_SETCLR | wanted);
    active = (UWORD)(custom->dmaconr & BENCH_MANAGED_DMA);
    Enable();

    if (active != wanted) {
        return restore_batch_state(state) && 0;
    }
    if (profile->use_blitter != 0U) {
        launch_blitter_copy();
    }
    return 1;
}

static int end_batch(const struct DmaProfile *profile,
                     const struct BatchState *state)
{
    int success = 1;

    if (profile->use_blitter != 0U) {
        if (!wait_for_blitter() ||
            fnv1a32(blitter_destination, BENCH_BLITTER_BYTES) !=
                expected_blitter_checksum) {
            success = 0;
        }
    }
    /* Do not change display DMA in the middle of a visible scan line. */
    if (!wait_for_raster_start()) {
        success = 0;
    }
    if (!restore_batch_state(state)) {
        success = 0;
    }
    if (!stack_guards_intact()) {
        success = 0;
    }
    return success;
}

static void fill_raw_source(void)
{
    size_t index;

    for (index = 0U; index < BENCH_RAW_BUFFER_BYTES; ++index) {
        raw_source[index] = (UBYTE)((index * 37U + 11U) & 0xffU);
    }
}

static void prepare_raw_destination(enum RawOperation operation)
{
    memset(raw_destination,
           operation == RAW_COPY_LONG4 ? 0U : (UBYTE)0x96U,
           BENCH_RAW_BUFFER_BYTES);
}

static ULONG expected_raw_write_checksum(enum RawOperation operation,
                                         ULONG seed)
{
    ULONG hash = 2166136261UL;
    size_t index;

    for (index = 0U; index < BENCH_RAW_BUFFER_BYTES; ++index) {
        unsigned int shift = 0U;
        UBYTE value;

        if (operation == RAW_WRITE_WORD) {
            shift = (index & 1U) == 0U ? 8U : 0U;
        } else if (operation == RAW_WRITE_LONG ||
                   operation == RAW_WRITE_LONG4) {
            shift = (unsigned int)((3U - (index & 3U)) * 8U);
        }
        value = (UBYTE)(seed >> shift);
        hash ^= (ULONG)value;
        hash *= 16777619UL;
    }
    return hash;
}

static ULONG expected_raw_read_checksum(void)
{
    ULONG checksum = 0U;
    size_t offset;

    for (offset = 0U; offset < BENCH_RAW_BUFFER_BYTES; offset += 4U) {
        checksum += ((ULONG)raw_source[offset] << 24) |
                    ((ULONG)raw_source[offset + 1U] << 16) |
                    ((ULONG)raw_source[offset + 2U] << 8) |
                    (ULONG)raw_source[offset + 3U];
    }
    return checksum * BENCH_RAW_ITERATIONS;
}

static int run_raw_case(const struct DmaProfile *dma,
                        const struct RawKernel *kernel,
                        struct RawResult *result)
{
    ULONG ticks[BENCH_RAW_SAMPLES];
    ULONG attempt;
    ULONG sample = 0U;

    prepare_raw_destination(kernel->operation);
    result->dma = dma;
    result->kernel = kernel;
    attempt = 0U;
    while (sample < BENCH_RAW_SAMPLES) {
        struct BatchState state;
        struct EClockVal start;
        struct EClockVal end;
        ULONG batch_return = 0U;
        ULONG measured;
        ULONG iteration;

        if (attempt >= BENCH_RAW_SAMPLES + 1U +
                           BENCH_TIMER_RETRY_LIMIT) {
            return 0;
        }
        if (!begin_batch(dma, &state)) {
            return 0;
        }
        (void)ReadEClock(&start);
        for (iteration = 0U; iteration < BENCH_RAW_ITERATIONS;
             ++iteration) {
            batch_return += kernel->kernel(
                raw_destination, raw_source, BENCH_RAW_BUFFER_BYTES,
                kernel->seed);
        }
        (void)ReadEClock(&end);
        if (!end_batch(dma, &state)) {
            return 0;
        }

        kernel_sink ^= batch_return;
        result->kernel_return_checksum = batch_return;
        if (attempt != 0U &&
            measured_elapsed_ticks(&start, &end, &measured)) {
            ticks[sample++] = measured;
        } else if (attempt != 0U) {
            ++timer_discarded_samples;
        }
        ++attempt;
    }

    sort_ticks(ticks, BENCH_RAW_SAMPLES);
    result->minimum_ticks = ticks[0];
    result->median_ticks = ticks[BENCH_RAW_SAMPLES / 2U];
    result->maximum_ticks = ticks[BENCH_RAW_SAMPLES - 1U];
    result->deadline_misses = 0U;
    for (sample = 0U; sample < BENCH_RAW_SAMPLES; ++sample) {
        if (ticks[sample] > frame_budget_ticks) {
            ++result->deadline_misses;
        }
    }

    if (kernel->operation == RAW_COPY_LONG4 ||
        kernel->operation == RAW_READ_LONG4) {
        result->expected_checksum =
            fnv1a32(raw_source, BENCH_RAW_BUFFER_BYTES);
        result->actual_checksum =
            fnv1a32(kernel->operation == RAW_COPY_LONG4
                        ? raw_destination
                        : raw_source,
                    BENCH_RAW_BUFFER_BYTES);
    } else {
        result->expected_checksum =
            expected_raw_write_checksum(kernel->operation, kernel->seed);
        result->actual_checksum =
            fnv1a32(raw_destination, BENCH_RAW_BUFFER_BYTES);
    }
    if (result->expected_checksum != result->actual_checksum) {
        return 0;
    }
    return kernel->operation != RAW_READ_LONG4 ||
           result->kernel_return_checksum == expected_raw_read_checksum();
}

static UBYTE source_color(ULONG x, ULONG y)
{
    UBYTE color =
        (UBYTE)(((x * 3U) + (y * 5U) + (x ^ y)) & 0x0fU);

    if (((x + (y * 3U)) & 7U) < 3U) {
        color = 0U;
    }
    return color;
}

static void build_c2p_sources(void)
{
    ULONG x;
    ULONG y;

    for (y = 0U; y < BENCH_SCREEN_HEIGHT; ++y) {
        for (x = 0U; x < BENCH_SCREEN_WIDTH; x += 2U) {
            UBYTE left = source_color(x, y);
            UBYTE right = source_color(x + 1U, y);

            packed_source[(y * (BENCH_SCREEN_WIDTH >> 1)) + (x >> 1)] =
                (UBYTE)((left << 4) | right);
            byte_source[(y * BENCH_SCREEN_WIDTH) + x] =
                (UBYTE)(0xa0U | left);
            byte_source[(y * BENCH_SCREEN_WIDTH) + x + 1U] =
                (UBYTE)(0xa0U | right);
        }
    }
}

static void clear_front_planes(void)
{
    size_t logical_plane;

    for (logical_plane = 0U; logical_plane < MAGI80_C2P4_PLANE_COUNT;
         ++logical_plane) {
        memset(physical_planes[logical_plane << 1], 0,
               BENCH_SCREEN_PLANE_BYTES);
    }
}

static enum Magi80C2P4Status convert_c2p(
    const struct C2PProfile *profile,
    enum C2PLayout layout,
    enum C2PBackend backend)
{
    const UBYTE *source =
        layout == C2P_LAYOUT_PACKED4 ? packed_source : byte_source;
    size_t source_stride =
        layout == C2P_LAYOUT_PACKED4
            ? (size_t)(BENCH_SCREEN_WIDTH >> 1)
            : (size_t)BENCH_SCREEN_WIDTH;
    UBYTE *planes[MAGI80_C2P4_PLANE_COUNT];
    size_t logical_plane;

    for (logical_plane = 0U; logical_plane < MAGI80_C2P4_PLANE_COUNT;
         ++logical_plane) {
        planes[logical_plane] = physical_planes[logical_plane << 1];
    }
    if (backend == C2P_BACKEND_MASK32_M68K) {
        if (layout == C2P_LAYOUT_PACKED4) {
            return magi80_c2p4_mask32_m68k_packed4(
                source, profile->width, profile->height, source_stride,
                planes, BENCH_SCREEN_WIDTH >> 3);
        }
        return magi80_c2p4_mask32_m68k_byte4(
            source, profile->width, profile->height, source_stride,
            planes, BENCH_SCREEN_WIDTH >> 3);
    }
    if (layout == C2P_LAYOUT_PACKED4) {
        return magi80_c2p4_pair_lut_m68k_packed4(
            source, profile->width, profile->height, source_stride,
            planes, BENCH_SCREEN_WIDTH >> 3, pair_lut);
    }
    return magi80_c2p4_pair_lut_m68k_byte4(
        source, profile->width, profile->height, source_stride, planes,
        BENCH_SCREEN_WIDTH >> 3, pair_lut);
}

static int prepare_c2p_oracles(void)
{
    UBYTE *oracle_planes[MAGI80_C2P4_PLANE_COUNT];
    size_t plane;
    size_t profile_index;
    size_t layout_index;

    for (plane = 0U; plane < MAGI80_C2P4_PLANE_COUNT; ++plane) {
        oracle_planes[plane] =
            oracle_planes_allocation + (plane * BENCH_SCREEN_PLANE_BYTES);
    }
    for (profile_index = 0U; profile_index < BENCH_C2P_PROFILE_COUNT;
         ++profile_index) {
        const struct C2PProfile *profile = &c2p_profiles[profile_index];

        for (layout_index = 0U; layout_index < BENCH_C2P_LAYOUT_COUNT;
             ++layout_index) {
            enum Magi80C2P4Status status;

            memset(oracle_planes_allocation, 0,
                   BENCH_SCREEN_PLANE_BYTES * MAGI80_C2P4_PLANE_COUNT);
            if (layout_index == C2P_LAYOUT_PACKED4) {
                status = magi80_c2p4_reference_packed4(
                    packed_source, profile->width, profile->height,
                    BENCH_SCREEN_WIDTH >> 1, oracle_planes,
                    BENCH_SCREEN_WIDTH >> 3);
            } else {
                status = magi80_c2p4_reference_byte4(
                    byte_source, profile->width, profile->height,
                    BENCH_SCREEN_WIDTH, oracle_planes,
                    BENCH_SCREEN_WIDTH >> 3);
            }
            if (status != MAGI80_C2P4_OK) {
                return 0;
            }
            c2p_expected_checksum[profile_index][layout_index] =
                hash_contiguous_c2p_planes(oracle_planes_allocation);
        }
    }
    return 1;
}

static int run_c2p_case(const struct DmaProfile *dma,
                        const struct C2PProfile *profile,
                        size_t profile_index,
                        enum C2PLayout layout,
                        enum C2PBackend backend,
                        struct C2PResult *result)
{
    ULONG ticks[BENCH_C2P_SAMPLES];
    ULONG attempt;
    ULONG sample = 0U;
    ULONG pixels = profile->width * profile->height;

    clear_front_planes();
    result->dma = dma;
    result->profile = profile;
    result->layout = layout;
    result->backend = backend;
    attempt = 0U;
    while (sample < BENCH_C2P_SAMPLES) {
        struct BatchState state;
        struct EClockVal start;
        struct EClockVal end;
        ULONG measured;
        ULONG iteration;

        if (attempt >= BENCH_C2P_SAMPLES + 1U +
                           BENCH_TIMER_RETRY_LIMIT) {
            return 0;
        }
        if (!begin_batch(dma, &state)) {
            return 0;
        }
        (void)ReadEClock(&start);
        for (iteration = 0U; iteration < BENCH_C2P_ITERATIONS;
             ++iteration) {
            if (convert_c2p(profile, layout, backend) !=
                MAGI80_C2P4_OK) {
                (void)end_batch(dma, &state);
                return 0;
            }
        }
        (void)ReadEClock(&end);
        if (!end_batch(dma, &state)) {
            return 0;
        }
        if (attempt != 0U &&
            measured_elapsed_ticks(&start, &end, &measured)) {
            ticks[sample++] = measured;
        } else if (attempt != 0U) {
            ++timer_discarded_samples;
        }
        ++attempt;
    }

    sort_ticks(ticks, BENCH_C2P_SAMPLES);
    result->minimum_ticks = ticks[0];
    result->median_ticks = ticks[BENCH_C2P_SAMPLES / 2U];
    result->maximum_ticks = ticks[BENCH_C2P_SAMPLES - 1U];
    result->deadline_misses = 0U;
    for (sample = 0U; sample < BENCH_C2P_SAMPLES; ++sample) {
        if (ticks[sample] > frame_budget_ticks) {
            ++result->deadline_misses;
        }
    }
    result->source_bytes =
        layout == C2P_LAYOUT_PACKED4 ? pixels >> 1 : pixels;
    result->plane_write_bytes = pixels >> 1;
    result->lookup_traffic_bytes =
        backend == C2P_BACKEND_PAIR_LUT_M68K ? pixels * 2U : 0U;
    result->minimum_chip_traffic_bytes =
        result->source_bytes + result->plane_write_bytes +
        result->lookup_traffic_bytes;
    result->expected_checksum =
        c2p_expected_checksum[profile_index][layout];
    result->actual_checksum = hash_front_planes(physical_planes);
    return result->expected_checksum == result->actual_checksum;
}

static int run_exclusive_suite(void)
{
    size_t dma_index;
    size_t raw_index;
    size_t c2p_profile_index;
    size_t layout_index;
    size_t backend_index;
    size_t raw_result_index = 0U;
    size_t c2p_result_index = 0U;
    int success = 1;

    dedicated_stack_pointer_offset =
        magi80_current_stack_pointer() -
        (ULONG)(uintptr_t)(stack_allocation + BENCH_STACK_GUARD_BYTES +
                           BENCH_STACK_BOUNDARY_RESERVE_BYTES);
    progress_phase = PHASE_EXCLUSIVE_ENTER;
    Forbid();
    for (dma_index = 0U; dma_index < BENCH_DMA_PROFILE_COUNT;
         ++dma_index) {
        for (raw_index = 0U; raw_index < BENCH_RAW_KERNEL_COUNT;
             ++raw_index) {
            progress_phase = PHASE_EXCLUSIVE_CASE;
            progress_case = (ULONG)(raw_result_index + c2p_result_index);
            if (!run_raw_case(&dma_profiles[dma_index],
                              &raw_kernels[raw_index],
                              &raw_results[raw_result_index++])) {
                success = 0;
                goto leave;
            }
        }
        for (c2p_profile_index = 0U;
             c2p_profile_index < BENCH_C2P_PROFILE_COUNT;
             ++c2p_profile_index) {
            for (layout_index = 0U;
                 layout_index < BENCH_C2P_LAYOUT_COUNT; ++layout_index) {
                for (backend_index = 0U;
                     backend_index < BENCH_C2P_BACKEND_COUNT;
                     ++backend_index) {
                    progress_phase = PHASE_EXCLUSIVE_CASE;
                    progress_case =
                        (ULONG)(raw_result_index + c2p_result_index);
                    if (!run_c2p_case(
                            &dma_profiles[dma_index],
                            &c2p_profiles[c2p_profile_index],
                            c2p_profile_index, (enum C2PLayout)layout_index,
                            (enum C2PBackend)backend_index,
                            &c2p_results[c2p_result_index++])) {
                        success = 0;
                        goto leave;
                    }
                }
            }
        }
    }

leave:
    if (!stack_guards_intact()) {
        success = 0;
    }
    progress_phase = PHASE_EXCLUSIVE_LEAVE;
    Permit();
    return success;
}

static void __attribute__((noinline)) run_on_dedicated_stack(void)
{
    progress_phase = PHASE_STACK_ENTER;
    StackSwap(&stack_swap);
    suite_status = run_exclusive_suite();
    StackSwap(&stack_swap);
    progress_phase = PHASE_STACK_LEAVE;
}

static int write_bytes(BPTR output, const char *text, size_t length)
{
    return output != (BPTR)0 && length <= 0x7fffffffUL &&
           Write(output, text, (LONG)length) == (LONG)length;
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
    } while (value != 0U);
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

static const char *active_token(UWORD bits, UWORD mask)
{
    return (bits & mask) != 0U ? "active" : "inactive";
}

static const char *blitter_token(const struct DmaProfile *profile)
{
    if (profile->use_blitter == 0U) {
        return "inactive";
    }
    return profile->blitter_hog != 0U ? "busy_hog" : "busy_fair";
}

static int write_dma_fields(BPTR output, const struct DmaProfile *profile)
{
    UWORD bits = profile_dma_bits(profile);
    ULONG display_fetch =
        (bits & DMAF_RASTER) != 0U ? BENCH_DISPLAY_BYTES : 0U;

    return write_text(output, " dma_profile=") &&
           write_text(output, profile->name) &&
           write_text(output, " display_state=") &&
           write_text(output,
                      (bits & DMAF_RASTER) != 0U ? "active" : "blanked") &&
           write_text(output, " display_dma=") &&
           write_text(output, active_token(bits, DMAF_RASTER)) &&
           write_text(output, " copper_dma=") &&
           write_text(output, active_token(bits, DMAF_COPPER)) &&
           write_text(output, " sprite_dma=") &&
           write_text(output, active_token(bits, DMAF_SPRITE)) &&
           write_text(output, " audio_dma=") &&
           write_text(output, active_token(bits, DMAF_AUDIO)) &&
           write_text(output, " blitter_dma=") &&
           write_text(output, blitter_token(profile)) &&
           write_key_decimal(
               output, " display_plane_fetch_bytes_per_video_frame=",
               display_fetch);
}

static int write_header(BPTR output)
{
    return write_text(output,
                      "exclusive_graphics_benchmark_format=1\n"
                      "benchmark=chipram_c2p4\n"
                      "environment=" MAGI80_BENCHMARK_ENVIRONMENT "\n"
                      "timing_authority=" MAGI80_BENCHMARK_AUTHORITY "\n"
                      "timing_scope=exclusive_kernel_batch\n"
                      "timing_source=eclock\n") &&
           write_key_decimal(output, "exec_version=", exec_version) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "exec_revision=", exec_revision) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "attention_flags=", attention_flags) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "power_supply_hz=", power_supply_hz) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "available_chip_bytes_before_setup=",
                             available_chip_bytes_before_setup) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "available_fast_bytes_before_setup=",
                             available_fast_bytes_before_setup) &&
           write_text(output, "\ndetected_stock_constraints=") &&
           write_text(output,
                      detected_stock_constraints != 0U ? "pass" : "fail") &&
           write_text(output, "\n") &&
           write_text(output, "initial_instruction_cache=") &&
           write_text(output,
                      (initial_cache_bits & CACRF_EnableI) != 0U
                          ? "active"
                          : "inactive") &&
           write_text(output, "\nbenchmark_instruction_cache=active\n") &&
           write_key_decimal(output, "raster_timeout_count=",
                             raster_timeout_count) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "eclock_hz=", eclock_hz) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "timer_overhead_ticks=",
                             timer_overhead_ticks) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "timer_discarded_samples=",
                             timer_discarded_samples) &&
           write_text(output,
                      "\ninterrupt_mode=custom_intena_masked\n"
                      "source_memory=chip\n"
                      "destination_memory=chip\n"
                      "display_mode=pal_256x256_dualpf_8plane\n"
                      "video_hz=50\n"
                      "screen_owner=intuition\n"
                      "publication=direct_visible_pf1\n"
                      "sprite_load=screen_managed\n"
                      "audio_load=four_channel_period124_muted\n"
                      "blitter_load=single_copy_a_to_d\n") &&
           write_key_decimal(output, "raster_start_line=",
                             BENCH_RASTER_START_LINE) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "dma_profile_count=",
                             BENCH_DMA_PROFILE_COUNT) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "audio_channels=",
                             BENCH_AUDIO_CHANNELS) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "blitter_copy_bytes=",
                             BENCH_BLITTER_BYTES) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "dedicated_stack_bytes=",
                             BENCH_STACK_BYTES) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "stack_boundary_reserve_bytes=",
                             BENCH_STACK_BOUNDARY_RESERVE_BYTES) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "stack_guard_bytes=",
                             BENCH_STACK_GUARD_BYTES) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "stack_high_water_bytes=",
                             stack_high_water_bytes) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "initial_stack_bytes=",
                             initial_stack_bytes) &&
           write_text(output,
                      "\ninterrupt_restore=pass\n"
                      "dma_restore=pass\n"
                      "checksum_algorithm=fnv1a32\n") &&
           write_key_decimal(output, "case_count=", BENCH_CASE_COUNT) &&
           write_text(output, "\n");
}

static int write_timing_fields(BPTR output, ULONG iterations,
                               ULONG samples, ULONG bytes_per_batch,
                               ULONG minimum_chip_traffic_bytes,
                               ULONG minimum_ticks, ULONG median_ticks,
                               ULONG maximum_ticks, ULONG misses,
                               ULONG expected_checksum,
                               ULONG actual_checksum)
{
    return write_key_decimal(output, " batch_iterations=", iterations) &&
           write_key_decimal(output, " samples=", samples) &&
           write_key_decimal(output, " bytes_per_batch=",
                             bytes_per_batch) &&
           write_key_decimal(output, " minimum_chip_traffic_bytes=",
                             minimum_chip_traffic_bytes) &&
           write_key_decimal(output, " frame_budget_ticks=",
                             frame_budget_ticks) &&
           write_key_decimal(output, " minimum_ticks=", minimum_ticks) &&
           write_key_decimal(output, " median_ticks=", median_ticks) &&
           write_key_decimal(output, " maximum_ticks=", maximum_ticks) &&
           write_key_decimal(output, " deadline_misses=", misses) &&
           write_key_decimal(output, " expected_checksum=",
                             expected_checksum) &&
           write_key_decimal(output, " actual_checksum=", actual_checksum);
}

static int write_raw_result(BPTR output, const struct RawResult *result)
{
    ULONG bytes = BENCH_RAW_BUFFER_BYTES * BENCH_RAW_ITERATIONS;

    return write_text(output, "case=") &&
           write_text(output, result->dma->name) &&
           write_text(output, "_raw_") &&
           write_text(output, result->kernel->name) &&
           write_text(output, " kind=raw") &&
           write_dma_fields(output, result->dma) &&
           write_text(output, " operation=") &&
           write_text(output, result->kernel->operation_name) &&
           write_text(output, " access_width=") &&
           write_text(output, result->kernel->access_width) &&
           write_timing_fields(
               output, BENCH_RAW_ITERATIONS, BENCH_RAW_SAMPLES, bytes,
               bytes * result->kernel->traffic_multiplier,
               result->minimum_ticks, result->median_ticks,
               result->maximum_ticks, result->deadline_misses,
               result->expected_checksum, result->actual_checksum) &&
           write_key_decimal(output, " kernel_return_checksum=",
                             result->kernel_return_checksum) &&
           write_text(output, " result=pass\n");
}

static const char *layout_name(enum C2PLayout layout)
{
    return layout == C2P_LAYOUT_PACKED4 ? "packed4" : "byte4";
}

static const char *backend_name(enum C2PBackend backend)
{
    return backend == C2P_BACKEND_PAIR_LUT_M68K
               ? "pair_lut_m68k"
               : "mask32_m68k";
}

static int write_c2p_result(BPTR output, const struct C2PResult *result)
{
    return write_text(output, "case=") &&
           write_text(output, result->dma->name) &&
           write_text(output, "_c2p4_") &&
           write_text(output, layout_name(result->layout)) &&
           write_text(output, "_") &&
           write_text(output, result->profile->name) &&
           write_text(output, "_") &&
           write_text(output, backend_name(result->backend)) &&
           write_text(output, " kind=c2p4") &&
           write_dma_fields(output, result->dma) &&
           write_text(output, " source_layout=") &&
           write_text(output, layout_name(result->layout)) &&
           write_text(output, " backend=") &&
           write_text(output, backend_name(result->backend)) &&
           write_key_decimal(output, " width=", result->profile->width) &&
           write_key_decimal(output, " height=", result->profile->height) &&
           write_key_decimal(output, " source_bytes=",
                             result->source_bytes) &&
           write_key_decimal(output, " plane_write_bytes=",
                             result->plane_write_bytes) &&
           write_key_decimal(output, " lookup_traffic_bytes=",
                             result->lookup_traffic_bytes) &&
           write_timing_fields(
               output, BENCH_C2P_ITERATIONS, BENCH_C2P_SAMPLES,
               result->plane_write_bytes,
               result->minimum_chip_traffic_bytes,
               result->minimum_ticks, result->median_ticks,
               result->maximum_ticks, result->deadline_misses,
               result->expected_checksum, result->actual_checksum) &&
           write_text(output, " destination=pf1 result=pass\n");
}

static int report_failure(BPTR output, const char *failure)
{
    (void)write_text(output, "benchmark=chipram_c2p4\nfailure=");
    (void)write_text(output, failure);
    (void)write_text(output, "\nexec_version=");
    (void)write_decimal(output, exec_version);
    (void)write_text(output, "\nexec_revision=");
    (void)write_decimal(output, exec_revision);
    (void)write_text(output, "\nattention_flags=");
    (void)write_decimal(output, attention_flags);
    (void)write_text(output, "\npower_supply_hz=");
    (void)write_decimal(output, power_supply_hz);
    (void)write_text(output, "\navailable_chip_bytes_before_setup=");
    (void)write_decimal(output, available_chip_bytes_before_setup);
    (void)write_text(output, "\navailable_fast_bytes_before_setup=");
    (void)write_decimal(output, available_fast_bytes_before_setup);
    (void)write_text(output, "\ndetected_stock_constraints=");
    (void)write_text(output,
                     detected_stock_constraints != 0U ? "pass" : "fail");
    (void)write_text(output, "\ninitial_instruction_cache=");
    (void)write_text(output,
                     (initial_cache_bits & CACRF_EnableI) != 0U
                         ? "active"
                         : "inactive");
    (void)write_text(output, "\nbenchmark_instruction_cache=active");
    (void)write_text(output, "\nlast_phase=");
    (void)write_decimal(output, progress_phase);
    (void)write_text(output, "\nlast_case=");
    (void)write_decimal(output, progress_case);
    (void)write_text(output, "\nstack_inspection_code=");
    (void)write_decimal(output, stack_inspection_code);
    (void)write_text(output, "\ndedicated_stack_pointer_offset=");
    (void)write_decimal(output, dedicated_stack_pointer_offset);
    (void)write_text(output, "\nstack_high_water_bytes=");
    (void)write_decimal(output, stack_high_water_bytes);
    (void)write_text(output, "\nstack_guard_mismatch_index=");
    (void)write_decimal(output, stack_guard_mismatch_index);
    (void)write_text(output, "\nstack_guard_mismatch_value=");
    (void)write_decimal(output, stack_guard_mismatch_value);
    (void)write_text(output, "\nblitter_timeout_count=");
    (void)write_decimal(output, blitter_timeout_count);
    (void)write_text(output, "\nraster_timeout_count=");
    (void)write_decimal(output, raster_timeout_count);
    (void)write_text(output, "\ntimer_discarded_samples=");
    (void)write_decimal(output, timer_discarded_samples);
    (void)write_text(output, "\nresult=fail\n");
    return RETURN_ERROR;
}

static int start_audio_load(void)
{
    size_t channel;

    audio_port = CreateMsgPort();
    if (audio_port == NULL) {
        return 0;
    }
    audio_control =
        (struct IOAudio *)CreateIORequest(audio_port,
                                         sizeof(struct IOAudio));
    if (audio_control == NULL) {
        return 0;
    }
    audio_control->ioa_Request.io_Message.mn_Node.ln_Pri =
        ADALLOC_MINPREC;
    audio_control->ioa_Request.io_Flags = ADIOF_NOWAIT;
    audio_control->ioa_Data = audio_allocation_map;
    audio_control->ioa_Length = sizeof(audio_allocation_map);
    if (OpenDevice(AUDIONAME, 0U,
                   (struct IORequest *)audio_control, 0U) != 0) {
        return 0;
    }
    if (((ULONG)(uintptr_t)audio_control->ioa_Request.io_Unit &
         DMAF_AUDIO) != DMAF_AUDIO) {
        return 0;
    }

    for (channel = 0U; channel < BENCH_AUDIO_CHANNELS; ++channel) {
        struct IOAudio *request = &audio_play[channel];

        memset(request, 0, sizeof(*request));
        request->ioa_Request.io_Message.mn_ReplyPort = audio_port;
        request->ioa_Request.io_Message.mn_Length = sizeof(*request);
        request->ioa_Request.io_Device =
            audio_control->ioa_Request.io_Device;
        request->ioa_Request.io_Unit =
            (struct Unit *)(uintptr_t)(1UL << channel);
        request->ioa_Request.io_Command = CMD_WRITE;
        request->ioa_Request.io_Flags = ADIOF_PERVOL;
        request->ioa_AllocKey = audio_control->ioa_AllocKey;
        request->ioa_Data = audio_sample;
        request->ioa_Length = BENCH_AUDIO_SAMPLE_BYTES;
        request->ioa_Period = BENCH_AUDIO_PERIOD;
        request->ioa_Volume = 0U;
        request->ioa_Cycles = 0U;
        BeginIO((struct IORequest *)request);
        audio_play_started[channel] = 1U;
    }
    WaitTOF();
    WaitTOF();
    audio_dma_mask = (UWORD)(custom->dmaconr & DMAF_AUDIO);
    return audio_dma_mask == DMAF_AUDIO;
}

static void stop_audio_load(void)
{
    size_t channel;

    for (channel = 0U; channel < BENCH_AUDIO_CHANNELS; ++channel) {
        if (audio_play_started[channel] != 0U) {
            if (CheckIO((struct IORequest *)&audio_play[channel]) == NULL) {
                (void)AbortIO((struct IORequest *)&audio_play[channel]);
            }
            (void)WaitIO((struct IORequest *)&audio_play[channel]);
            audio_play_started[channel] = 0U;
        }
    }
    if (audio_control != NULL &&
        audio_control->ioa_Request.io_Device != NULL) {
        CloseDevice((struct IORequest *)audio_control);
    }
    if (audio_control != NULL) {
        DeleteIORequest((struct IORequest *)audio_control);
        audio_control = NULL;
    }
    if (audio_port != NULL) {
        DeleteMsgPort(audio_port);
        audio_port = NULL;
    }
}

int main(int argc, char **argv)
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
        {SA_Depth, BENCH_SCREEN_DEPTH},
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
    struct Task *task;
    struct Screen *screen = NULL;
    struct BitMap *bitmap;
    struct EClockVal now;
    BPTR console_output = Output();
    BPTR report_output = console_output;
    BPTR report_file = (BPTR)0;
    const char *report_path = MAGI80_BENCHMARK_REPORT_PATH;
    UBYTE *usable_stack;
    ULONG chip_revision;
    size_t plane;
    size_t index;
    int timer_open = 0;
    int blitter_owned = 0;
    const char *failure = NULL;

    if (argc == 3 && strcmp(argv[1], "--report") == 0) {
        report_path = argv[2];
    }
    if (report_path != NULL) {
        (void)write_text(
            console_output,
            "MAGI-80 exclusive graphics benchmark starting.\n"
            "Screen changes and silence are expected. Please wait up to five minutes.\n");
        report_file = Open(report_path, MODE_NEWFILE);
        if (report_file == (BPTR)0) {
            (void)write_text(console_output,
                             "MAGI-80 BENCHMARK RESULT: FAIL\n"
                             "The writable result file could not be created.\n");
            return RETURN_ERROR;
        }
        if (!write_text(report_file,
                        "benchmark=chipram_c2p4\nresult=running\n")) {
            (void)Close(report_file);
            report_file = (BPTR)0;
            (void)write_text(console_output,
                             "MAGI-80 BENCHMARK RESULT: FAIL\n"
                             "The writable result file could not be created.\n");
            return RETURN_ERROR;
        }
        if (!Close(report_file)) {
            report_file = (BPTR)0;
            (void)write_text(console_output,
                             "MAGI-80 BENCHMARK RESULT: FAIL\n"
                             "The writable result file could not be created.\n");
            return RETURN_ERROR;
        }
        report_file = (BPTR)0;
    }

    memset(&timer_request, 0, sizeof(timer_request));
    memset(raw_results, 0, sizeof(raw_results));
    memset(c2p_results, 0, sizeof(c2p_results));
    progress_phase = PHASE_HOSTED_PREPARE;

    exec_version = SysBase->LibNode.lib_Version;
    exec_revision = SysBase->LibNode.lib_Revision;
    attention_flags = SysBase->AttnFlags;
    power_supply_hz = SysBase->PowerSupplyFrequency;
    available_chip_bytes_before_setup = AvailMem(MEMF_CHIP);
    available_fast_bytes_before_setup = AvailMem(MEMF_FAST);
    detected_stock_constraints =
        (UBYTE)(((attention_flags & AFF_68020) != 0U &&
                 (attention_flags & (AFF_68030 | AFF_68040 | AFF_68060)) ==
                     0U &&
                 power_supply_hz == 50U &&
                 available_fast_bytes_before_setup == 0U)
                    ? 1U
                    : 0U);

    /* A normal Workbench startup enables the 68020 instruction cache via
     * SetPatch.  A self-contained boot floppy does not, so request the same
     * state through exec.library and restore the caller's global state later.
     */
    initial_cache_bits =
        CacheControl(CACRF_EnableI, CACRF_EnableI);
    cache_controlled = 1;

    task = FindTask(NULL);
    if (task == NULL || task->tc_SPUpper <= task->tc_SPLower) {
        failure = "inspect_initial_stack";
        goto cleanup;
    }
    initial_stack_bytes =
        (ULONG)((uintptr_t)task->tc_SPUpper -
                (uintptr_t)task->tc_SPLower);

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
    eclock_hz = ReadEClock(&now);
    timer_overhead_ticks = measure_timer_overhead();
    frame_budget_ticks = eclock_hz / 25U;

    screen = OpenScreenTagList(NULL, screen_tags);
    if (screen == NULL) {
        failure = "open_aga_screen";
        goto cleanup;
    }
    bitmap = screen->RastPort.BitMap;
    if (bitmap == NULL || bitmap->Depth != BENCH_SCREEN_DEPTH ||
        bitmap->BytesPerRow != BENCH_SCREEN_WIDTH / 8U) {
        failure = "screen_bitmap_layout";
        goto cleanup;
    }
    for (plane = 0U; plane < BENCH_SCREEN_DEPTH; ++plane) {
        if (bitmap->Planes[plane] == NULL ||
            (TypeOfMem(bitmap->Planes[plane]) & MEMF_CHIP) == 0U) {
            failure = "screen_chip_planes";
            goto cleanup;
        }
        physical_planes[plane] = (UBYTE *)bitmap->Planes[plane];
        memset(physical_planes[plane],
               (plane & 1U) != 0U ? (int)(0x11U << (plane >> 1)) : 0,
               BENCH_SCREEN_PLANE_BYTES);
    }

    raw_source =
        (UBYTE *)AllocMem(BENCH_RAW_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    raw_destination =
        (UBYTE *)AllocMem(BENCH_RAW_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    packed_source =
        (UBYTE *)AllocMem((BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT) >> 1,
                          MEMF_CHIP | MEMF_CLEAR);
    byte_source =
        (UBYTE *)AllocMem(BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT,
                          MEMF_CHIP | MEMF_CLEAR);
    pair_lut = (uint32_t *)AllocMem(
        MAGI80_C2P4_PAIR_LUT_ENTRIES * sizeof(uint32_t),
        MEMF_CHIP | MEMF_CLEAR);
    oracle_planes_allocation = (UBYTE *)AllocMem(
        BENCH_SCREEN_PLANE_BYTES * MAGI80_C2P4_PLANE_COUNT,
        MEMF_PUBLIC | MEMF_CLEAR);
    audio_sample = (UBYTE *)AllocMem(BENCH_AUDIO_SAMPLE_BYTES,
                                     MEMF_CHIP | MEMF_CLEAR);
    blitter_source =
        (UBYTE *)AllocMem(BENCH_BLITTER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    blitter_destination =
        (UBYTE *)AllocMem(BENCH_BLITTER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    stack_allocation =
        (UBYTE *)AllocMem(BENCH_STACK_TOTAL_BYTES, MEMF_PUBLIC);
    if (raw_source == NULL || raw_destination == NULL ||
        packed_source == NULL || byte_source == NULL || pair_lut == NULL ||
        oracle_planes_allocation == NULL || audio_sample == NULL ||
        blitter_source == NULL || blitter_destination == NULL ||
        stack_allocation == NULL) {
        failure = "allocate_benchmark_memory";
        goto cleanup;
    }
    if ((TypeOfMem(raw_source) & MEMF_CHIP) == 0U ||
        (TypeOfMem(raw_destination) & MEMF_CHIP) == 0U ||
        (TypeOfMem(packed_source) & MEMF_CHIP) == 0U ||
        (TypeOfMem(byte_source) & MEMF_CHIP) == 0U ||
        (TypeOfMem(pair_lut) & MEMF_CHIP) == 0U ||
        (TypeOfMem(audio_sample) & MEMF_CHIP) == 0U ||
        (TypeOfMem(blitter_source) & MEMF_CHIP) == 0U ||
        (TypeOfMem(blitter_destination) & MEMF_CHIP) == 0U) {
        failure = "verify_chip_memory";
        goto cleanup;
    }

    fill_raw_source();
    build_c2p_sources();
    magi80_c2p4_build_pair_lut(pair_lut);
    if (!prepare_c2p_oracles()) {
        failure = "prepare_c2p_oracles";
        goto cleanup;
    }
    for (index = 0U; index < BENCH_AUDIO_SAMPLE_BYTES; ++index) {
        audio_sample[index] = (UBYTE)((index * 29U) & 0xffU);
    }
    for (index = 0U; index < BENCH_BLITTER_BYTES; ++index) {
        blitter_source[index] = (UBYTE)((index * 13U + 7U) & 0xffU);
    }
    expected_blitter_checksum =
        fnv1a32(blitter_source, BENCH_BLITTER_BYTES);

    memset(stack_allocation, BENCH_STACK_LOWER_GUARD,
           BENCH_STACK_GUARD_BYTES);
    memset(stack_allocation + BENCH_STACK_GUARD_BYTES,
           BENCH_STACK_FILL,
           BENCH_STACK_BYTES +
               (2U * BENCH_STACK_BOUNDARY_RESERVE_BYTES));
    usable_stack = stack_allocation + BENCH_STACK_GUARD_BYTES +
                   BENCH_STACK_BOUNDARY_RESERVE_BYTES;
    memset(usable_stack + BENCH_STACK_BYTES +
               BENCH_STACK_BOUNDARY_RESERVE_BYTES,
           BENCH_STACK_UPPER_GUARD, BENCH_STACK_GUARD_BYTES);
    stack_swap.stk_Lower =
        usable_stack - BENCH_STACK_BOUNDARY_RESERVE_BYTES;
    stack_swap.stk_Upper =
        (ULONG)(uintptr_t)(usable_stack + BENCH_STACK_BYTES +
                           BENCH_STACK_BOUNDARY_RESERVE_BYTES);
    stack_swap.stk_Pointer =
        (APTR)(uintptr_t)(usable_stack + BENCH_STACK_BYTES);
    if (!stack_guards_intact()) {
        failure = "stack_guard_before_entry";
        goto cleanup;
    }

    if (!start_audio_load()) {
        failure = "start_four_channel_audio_load";
        goto cleanup;
    }
    OwnBlitter();
    blitter_owned = 1;
    WaitBlit();
    WaitTOF();
    WaitTOF();
    suite_initial_intena = custom->intenar;
    suite_initial_dma = custom->dmaconr;
    if ((suite_initial_dma & DMAF_MASTER) == 0U ||
        (suite_initial_dma & (DMAF_RASTER | DMAF_COPPER)) !=
            (DMAF_RASTER | DMAF_COPPER) ||
        (suite_initial_dma & DMAF_AUDIO) != DMAF_AUDIO) {
        failure = "initial_dma_state";
        goto cleanup;
    }

    run_on_dedicated_stack();
    progress_phase = PHASE_VALIDATE;
    if (!inspect_stack()) {
        failure = "stack_guard_or_high_water";
        goto cleanup;
    }
    if (!suite_status) {
        failure = "exclusive_suite";
        goto cleanup;
    }
    if ((custom->intenar & BENCH_ALL_CUSTOM_INTERRUPTS) !=
        (suite_initial_intena & BENCH_ALL_CUSTOM_INTERRUPTS)) {
        failure = "interrupt_restore";
        goto cleanup;
    }
    if ((custom->dmaconr & BENCH_MANAGED_DMA) !=
        (suite_initial_dma & BENCH_MANAGED_DMA)) {
        failure = "dma_restore";
        goto cleanup;
    }
    if (report_path != NULL) {
        report_file = Open(report_path, MODE_NEWFILE);
        if (report_file == (BPTR)0) {
            failure = "open_report_after_exclusive";
            goto cleanup;
        }
        report_output = report_file;
    }
    if (!write_header(report_output)) {
        failure = "write_report_header";
        goto cleanup;
    }
    for (index = 0U; index < BENCH_RAW_RESULT_COUNT; ++index) {
        if (!write_raw_result(report_output, &raw_results[index])) {
            failure = "write_raw_result";
            goto cleanup;
        }
    }
    for (index = 0U; index < BENCH_C2P_RESULT_COUNT; ++index) {
        if (!write_c2p_result(report_output, &c2p_results[index])) {
            failure = "write_c2p_result";
            goto cleanup;
        }
    }

cleanup:
    progress_phase = PHASE_CLEANUP;
    if (blitter_owned) {
        WaitBlit();
        DisownBlitter();
    }
    stop_audio_load();
    if (stack_allocation != NULL) {
        FreeMem(stack_allocation, BENCH_STACK_TOTAL_BYTES);
    }
    if (blitter_destination != NULL) {
        FreeMem(blitter_destination, BENCH_BLITTER_BYTES);
    }
    if (blitter_source != NULL) {
        FreeMem(blitter_source, BENCH_BLITTER_BYTES);
    }
    if (audio_sample != NULL) {
        FreeMem(audio_sample, BENCH_AUDIO_SAMPLE_BYTES);
    }
    if (oracle_planes_allocation != NULL) {
        FreeMem(oracle_planes_allocation,
                BENCH_SCREEN_PLANE_BYTES * MAGI80_C2P4_PLANE_COUNT);
    }
    if (pair_lut != NULL) {
        FreeMem(pair_lut,
                MAGI80_C2P4_PAIR_LUT_ENTRIES * sizeof(uint32_t));
    }
    if (byte_source != NULL) {
        FreeMem(byte_source, BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT);
    }
    if (packed_source != NULL) {
        FreeMem(packed_source,
                (BENCH_SCREEN_WIDTH * BENCH_SCREEN_HEIGHT) >> 1);
    }
    if (raw_destination != NULL) {
        FreeMem(raw_destination, BENCH_RAW_BUFFER_BYTES);
    }
    if (raw_source != NULL) {
        FreeMem(raw_source, BENCH_RAW_BUFFER_BYTES);
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
    if (cache_controlled) {
        (void)CacheControl(initial_cache_bits, CACRF_EnableI);
        cache_controlled = 0;
    }
    if (failure != NULL) {
        if (report_path != NULL && report_file == (BPTR)0) {
            report_file = Open(report_path, MODE_NEWFILE);
            if (report_file != (BPTR)0) {
                report_output = report_file;
            }
        }
        (void)report_failure(report_output, failure);
        if (report_file != (BPTR)0) {
            (void)Close(report_file);
            (void)write_text(console_output,
                             "\nMAGI-80 BENCHMARK RESULT: FAIL\n"
                             "Keep the disk and photograph this screen.\n"
                             "Diagnostic report: ");
            (void)write_text(console_output, report_path);
            (void)write_text(console_output, "\n");
        }
        return RETURN_ERROR;
    }
    progress_phase = PHASE_COMPLETE;
    if (!write_text(report_output, "result=pass\n")) {
        if (report_file != (BPTR)0) {
            (void)Close(report_file);
            (void)write_text(console_output,
                             "\nMAGI-80 BENCHMARK RESULT: FAIL\n"
                             "The report footer could not be written.\n");
        }
        return RETURN_ERROR;
    }
    if (report_file != (BPTR)0) {
        if (!Close(report_file)) {
            (void)write_text(console_output,
                             "\nMAGI-80 BENCHMARK RESULT: FAIL\n"
                             "The report file could not be closed.\n");
            return RETURN_ERROR;
        }
        (void)write_text(console_output,
                         "\nMAGI-80 BENCHMARK RESULT: PASS\n"
                         "Wait for the floppy LED to stop before ejecting.\n"
                         "Result report: ");
        (void)write_text(console_output, report_path);
        (void)write_text(console_output, "\n");
    }
    return RETURN_OK;
}
