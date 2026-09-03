#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <devices/timer.h>
#include <dos/dos.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>

#include "kernels.h"

#define BENCH_BUFFER_BYTES 8192U
#define BENCH_BATCH_ITERATIONS 4U
#define BENCH_SAMPLES 7U
#define BENCH_CASE_COUNT 6U
#define BENCH_STACK_BYTES 16384U
#define BENCH_STACK_BOUNDARY_RESERVE_BYTES 256U
#define BENCH_STACK_GUARD_BYTES 256U
#define BENCH_STACK_TOTAL_BYTES \
    (BENCH_STACK_BYTES + (2U * BENCH_STACK_BOUNDARY_RESERVE_BYTES) + \
     (2U * BENCH_STACK_GUARD_BYTES))
#define BENCH_STACK_FILL 0xcdU
#define BENCH_STACK_LOWER_GUARD 0x5aU
#define BENCH_STACK_UPPER_GUARD 0xa5U
#define BENCH_ALL_CUSTOM_INTERRUPTS 0x7fffU

#ifndef MAGI80_BENCHMARK_ENVIRONMENT
#define MAGI80_BENCHMARK_ENVIRONMENT "fs_uae_a1200_pal"
#endif

struct Device *TimerBase = NULL;

enum BenchmarkOperation {
    OP_WRITE_BYTE = 0,
    OP_WRITE_WORD,
    OP_WRITE_LONG,
    OP_WRITE_LONG4,
    OP_COPY_LONG4,
    OP_READ_LONG4
};

enum ProgressPhase {
    PHASE_HOSTED_PREPARE = 1,
    PHASE_STACK_ENTER = 2,
    PHASE_EXCLUSIVE_ENTER = 3,
    PHASE_CASE_BASE = 16,
    PHASE_EXCLUSIVE_LEAVE = 32,
    PHASE_STACK_LEAVE = 33,
    PHASE_VALIDATE = 34,
    PHASE_CLEANUP = 35,
    PHASE_COMPLETE = 36
};

struct BenchmarkCase {
    const char *name;
    const char *operation_name;
    const char *access_width;
    enum BenchmarkOperation operation;
    Magi80ChipRamKernel kernel;
    ULONG seed;
    ULONG traffic_multiplier;
};

struct BenchmarkResult {
    const struct BenchmarkCase *definition;
    ULONG minimum_ticks;
    ULONG median_ticks;
    ULONG maximum_ticks;
    ULONG deadline_misses;
    ULONG expected_checksum;
    ULONG actual_checksum;
    ULONG kernel_return_checksum;
    ULONG stack_high_water_bytes;
};

static const struct BenchmarkCase benchmark_cases[BENCH_CASE_COUNT] = {
    {"write_byte", "write", "byte", OP_WRITE_BYTE,
     magi80_chipram_write_byte, 0x00000012U, 1U},
    {"write_word", "write", "word", OP_WRITE_WORD,
     magi80_chipram_write_word, 0x00003456U, 1U},
    {"write_long", "write", "long", OP_WRITE_LONG,
     magi80_chipram_write_long, 0x89abcdefU, 1U},
    {"write_long4", "write", "long_unrolled4", OP_WRITE_LONG4,
     magi80_chipram_write_long4, 0x13579bdfU, 1U},
    {"copy_long4", "copy", "long_unrolled4", OP_COPY_LONG4,
     magi80_chipram_copy_long4, 0U, 2U},
    {"read_long4", "read", "long_unrolled4", OP_READ_LONG4,
     magi80_chipram_read_long4, 0U, 1U}
};

static struct timerequest timer_request;
static struct StackSwapStruct stack_swap;
static struct BenchmarkResult results[BENCH_CASE_COUNT];
static UBYTE *source_buffer;
static UBYTE *destination_buffer;
static UBYTE *stack_allocation;
static volatile ULONG kernel_sink;
static volatile ULONG progress_phase;
static ULONG eclock_hz;
static ULONG timer_overhead_ticks;
static ULONG frame_budget_ticks;
static ULONG initial_stack_bytes;
static ULONG saved_dma_state;
static UWORD initial_intena_state;
static ULONG stack_high_water_bytes;
static ULONG dedicated_stack_pointer_offset;
static ULONG stack_inspection_code;
static ULONG stack_guard_mismatch_index;
static ULONG stack_guard_mismatch_value;
static int suite_status;

static volatile struct Custom *const custom =
    (volatile struct Custom *)(uintptr_t)0x00dff000UL;

static ULONG elapsed_ticks(
    const struct EClockVal *start,
    const struct EClockVal *end)
{
    return end->ev_lo - start->ev_lo;
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

static void sort_ticks(ULONG values[BENCH_SAMPLES])
{
    size_t outer;

    for (outer = 1U; outer < BENCH_SAMPLES; ++outer) {
        ULONG value = values[outer];
        size_t inner = outer;

        while (inner > 0U && values[inner - 1U] > value) {
            values[inner] = values[inner - 1U];
            --inner;
        }
        values[inner] = value;
    }
}

static ULONG fnv1a32(const UBYTE *bytes, size_t length)
{
    ULONG hash = 2166136261UL;
    size_t index;

    for (index = 0U; index < length; ++index) {
        hash ^= (ULONG)bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

static void fill_source(void)
{
    size_t index;

    for (index = 0U; index < BENCH_BUFFER_BYTES; ++index) {
        source_buffer[index] = (UBYTE)((index * 37U + 11U) & 0xffU);
    }
}

static void prepare_destination(enum BenchmarkOperation operation)
{
    size_t index;

    for (index = 0U; index < BENCH_BUFFER_BYTES; ++index) {
        destination_buffer[index] =
            operation == OP_COPY_LONG4 ? 0U : (UBYTE)0x96U;
    }
}

static ULONG expected_write_checksum(enum BenchmarkOperation operation,
                                     ULONG seed)
{
    ULONG hash = 2166136261UL;
    size_t index;

    for (index = 0U; index < BENCH_BUFFER_BYTES; ++index) {
        unsigned int shift = 0U;
        UBYTE value;

        if (operation == OP_WRITE_WORD) {
            shift = (index & 1U) == 0U ? 8U : 0U;
        } else if (operation == OP_WRITE_LONG ||
                   operation == OP_WRITE_LONG4) {
            shift = (unsigned int)((3U - (index & 3U)) * 8U);
        }
        value = (UBYTE)(seed >> shift);
        hash ^= (ULONG)value;
        hash *= 16777619UL;
    }
    return hash;
}

static ULONG expected_read_checksum(void)
{
    ULONG checksum = 0U;
    size_t offset;

    for (offset = 0U; offset < BENCH_BUFFER_BYTES; offset += 4U) {
        checksum += ((ULONG)source_buffer[offset] << 24) |
                    ((ULONG)source_buffer[offset + 1U] << 16) |
                    ((ULONG)source_buffer[offset + 2U] << 8) |
                    (ULONG)source_buffer[offset + 3U];
    }
    return checksum * BENCH_BATCH_ITERATIONS;
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

static int verify_case(struct BenchmarkResult *result)
{
    const struct BenchmarkCase *definition = result->definition;

    if (definition->operation == OP_COPY_LONG4 ||
        definition->operation == OP_READ_LONG4) {
        result->expected_checksum =
            fnv1a32(source_buffer, BENCH_BUFFER_BYTES);
        result->actual_checksum =
            fnv1a32(definition->operation == OP_COPY_LONG4
                        ? destination_buffer
                        : source_buffer,
                    BENCH_BUFFER_BYTES);
    } else {
        result->expected_checksum = expected_write_checksum(
            definition->operation, definition->seed);
        result->actual_checksum =
            fnv1a32(destination_buffer, BENCH_BUFFER_BYTES);
    }
    if (result->expected_checksum != result->actual_checksum) {
        return 0;
    }
    if (definition->operation == OP_READ_LONG4 &&
        result->kernel_return_checksum != expected_read_checksum()) {
        return 0;
    }
    return 1;
}

static int run_case(size_t case_index)
{
    const struct BenchmarkCase *definition = &benchmark_cases[case_index];
    struct BenchmarkResult *result = &results[case_index];
    ULONG ticks[BENCH_SAMPLES];
    ULONG attempt;
    ULONG sample = 0U;

    result->definition = definition;
    result->kernel_return_checksum = 0U;
    prepare_destination(definition->operation);
    for (attempt = 0U; attempt <= BENCH_SAMPLES; ++attempt) {
        struct EClockVal start;
        struct EClockVal end;
        UWORD previous_intena;
        ULONG iteration;
        ULONG batch_return = 0U;

        Disable();
        previous_intena = custom->intenar;
        custom->intena = BENCH_ALL_CUSTOM_INTERRUPTS;
        Enable();

        (void)ReadEClock(&start);
        for (iteration = 0U; iteration < BENCH_BATCH_ITERATIONS;
             ++iteration) {
            batch_return += definition->kernel(
                destination_buffer, source_buffer, BENCH_BUFFER_BYTES,
                definition->seed);
        }
        (void)ReadEClock(&end);

        Disable();
        custom->intena = BENCH_ALL_CUSTOM_INTERRUPTS;
        custom->intena = (UWORD)(INTF_SETCLR |
                                 (previous_intena &
                                  BENCH_ALL_CUSTOM_INTERRUPTS));
        Enable();

        if (!stack_guards_intact()) {
            return 0;
        }

        kernel_sink ^= batch_return;
        if (attempt != 0U) {
            ticks[sample] = elapsed_ticks(&start, &end);
            ++sample;
        }
        result->kernel_return_checksum = batch_return;
    }

    sort_ticks(ticks);
    result->minimum_ticks = ticks[0];
    result->median_ticks = ticks[BENCH_SAMPLES / 2U];
    result->maximum_ticks = ticks[BENCH_SAMPLES - 1U];
    result->deadline_misses = 0U;
    for (sample = 0U; sample < BENCH_SAMPLES; ++sample) {
        if (ticks[sample] > frame_budget_ticks) {
            ++result->deadline_misses;
        }
    }
    return verify_case(result);
}

static int run_exclusive_suite(void)
{
    size_t case_index;
    int success = 1;

    dedicated_stack_pointer_offset =
        magi80_current_stack_pointer() -
        (ULONG)(uintptr_t)(stack_allocation + BENCH_STACK_GUARD_BYTES +
                           BENCH_STACK_BOUNDARY_RESERVE_BYTES);
    progress_phase = PHASE_EXCLUSIVE_ENTER;
    Forbid();
    for (case_index = 0U; case_index < BENCH_CASE_COUNT; ++case_index) {
        progress_phase = PHASE_CASE_BASE + (ULONG)case_index;
        if (!run_case(case_index)) {
            success = 0;
            break;
        }
    }
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

static int write_header(BPTR output)
{
    ULONG active_dma = saved_dma_state & DMAF_MASTER;

    return write_text(output, "chipram_benchmark_format=1\n") &&
           write_text(output, "environment=") &&
           write_text(output, MAGI80_BENCHMARK_ENVIRONMENT) &&
           write_text(output, "\n") &&
           write_text(output, "timing_authority=protocol_only\n") &&
           write_text(output,
                      "timing_scope=exclusive_kernel_batch\n") &&
           write_text(output, "timing_source=eclock\n") &&
           write_key_decimal(output, "eclock_hz=", eclock_hz) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "timer_overhead_ticks=",
                             timer_overhead_ticks) &&
           write_text(output, "\n") &&
           write_text(output, "display_dma=") &&
           write_text(output, active_dma != 0U &&
                                      (saved_dma_state & DMAF_RASTER) != 0U
                                  ? "active\n"
                                  : "inactive\n") &&
           write_text(output, "sprite_dma=") &&
           write_text(output, active_dma != 0U &&
                                      (saved_dma_state & DMAF_SPRITE) != 0U
                                  ? "active\n"
                                  : "inactive\n") &&
           write_text(output, "audio_dma=") &&
           write_text(output, active_dma != 0U &&
                                      (saved_dma_state & DMAF_AUDIO) != 0U
                                  ? "active\n"
                                  : "inactive\n") &&
           write_text(output,
                      "interrupt_mode=custom_intena_masked\n") &&
           write_key_decimal(output, "buffer_bytes=",
                             BENCH_BUFFER_BYTES) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "batch_iterations=",
                             BENCH_BATCH_ITERATIONS) &&
           write_text(output, "\n") &&
           write_key_decimal(output, "samples=", BENCH_SAMPLES) &&
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
           write_text(output, "\n") &&
           write_text(output, "interrupt_restore=pass\n") &&
           write_key_decimal(output, "case_count=", BENCH_CASE_COUNT) &&
           write_text(output, "\n");
}

static int write_case(BPTR output, const struct BenchmarkResult *result)
{
    const struct BenchmarkCase *definition = result->definition;
    ULONG batch_bytes = BENCH_BUFFER_BYTES * BENCH_BATCH_ITERATIONS;
    ULONG traffic_bytes = batch_bytes * definition->traffic_multiplier;
    return write_text(output, "case=") &&
           write_text(output, definition->name) &&
           write_text(output, " operation=") &&
           write_text(output, definition->operation_name) &&
           write_text(output, " access_width=") &&
           write_text(output, definition->access_width) &&
           write_key_decimal(output, " bytes_per_batch=", batch_bytes) &&
           write_key_decimal(output, " minimum_chip_traffic_bytes=",
                             traffic_bytes) &&
           write_key_decimal(output, " frame_budget_ticks=",
                             frame_budget_ticks) &&
           write_key_decimal(output, " minimum_ticks=",
                             result->minimum_ticks) &&
           write_key_decimal(output, " median_ticks=",
                             result->median_ticks) &&
           write_key_decimal(output, " maximum_ticks=",
                             result->maximum_ticks) &&
           write_key_decimal(output, " expected_checksum=",
                             result->expected_checksum) &&
           write_key_decimal(output, " actual_checksum=",
                             result->actual_checksum) &&
           write_key_decimal(output, " deadline_misses=",
                             result->deadline_misses) &&
           write_key_decimal(output, " kernel_return_checksum=",
                             result->kernel_return_checksum) &&
           write_text(output, " result=pass\n");
}

static int report_failure(const char *failure)
{
    BPTR output = Output();

    (void)write_text(output, "benchmark=chipram\nfailure=");
    (void)write_text(output, failure);
    (void)write_text(output, "\nlast_phase=");
    (void)write_decimal(output, progress_phase);
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
    (void)write_text(output, "\nresult=fail\n");
    return RETURN_ERROR;
}

int main(void)
{
    struct Task *task;
    UBYTE *usable_stack;
    size_t case_index;
    int timer_open = 0;
    const char *failure = NULL;

    memset(&timer_request, 0, sizeof(timer_request));
    memset(results, 0, sizeof(results));
    progress_phase = PHASE_HOSTED_PREPARE;

    task = FindTask(NULL);
    if (task == NULL || task->tc_SPUpper <= task->tc_SPLower) {
        failure = "inspect_initial_stack";
        goto cleanup;
    }
    initial_stack_bytes =
        (ULONG)((uintptr_t)task->tc_SPUpper -
                (uintptr_t)task->tc_SPLower);
    if (OpenDevice(TIMERNAME, UNIT_ECLOCK,
                   (struct IORequest *)&timer_request, 0U) != 0) {
        failure = "open_timer_device";
        goto cleanup;
    }
    timer_open = 1;
    TimerBase = timer_request.tr_node.io_Device;
    {
        struct EClockVal now;

        eclock_hz = ReadEClock(&now);
    }
    timer_overhead_ticks = measure_timer_overhead();
    frame_budget_ticks = eclock_hz / 25U;
    saved_dma_state = custom->dmaconr;
    initial_intena_state = custom->intenar;

    source_buffer =
        (UBYTE *)AllocMem(BENCH_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    destination_buffer =
        (UBYTE *)AllocMem(BENCH_BUFFER_BYTES, MEMF_CHIP | MEMF_CLEAR);
    stack_allocation =
        (UBYTE *)AllocMem(BENCH_STACK_TOTAL_BYTES, MEMF_PUBLIC);
    if (source_buffer == NULL || destination_buffer == NULL ||
        stack_allocation == NULL ||
        (TypeOfMem(source_buffer) & MEMF_CHIP) == 0U ||
        (TypeOfMem(destination_buffer) & MEMF_CHIP) == 0U) {
        failure = "allocate_benchmark_memory";
        goto cleanup;
    }

    fill_source();
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
           BENCH_STACK_UPPER_GUARD,
           BENCH_STACK_GUARD_BYTES);
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
        (initial_intena_state & BENCH_ALL_CUSTOM_INTERRUPTS)) {
        failure = "interrupt_restore";
        goto cleanup;
    }
    for (case_index = 0U; case_index < BENCH_CASE_COUNT; ++case_index) {
        results[case_index].stack_high_water_bytes =
            stack_high_water_bytes;
    }
    if (!write_header(Output())) {
        failure = "write_report_header";
        goto cleanup;
    }
    for (case_index = 0U; case_index < BENCH_CASE_COUNT; ++case_index) {
        if (!write_case(Output(), &results[case_index])) {
            failure = "write_report_case";
            goto cleanup;
        }
    }

cleanup:
    progress_phase = PHASE_CLEANUP;
    if (stack_allocation != NULL) {
        FreeMem(stack_allocation, BENCH_STACK_TOTAL_BYTES);
        stack_allocation = NULL;
    }
    if (destination_buffer != NULL) {
        FreeMem(destination_buffer, BENCH_BUFFER_BYTES);
        destination_buffer = NULL;
    }
    if (source_buffer != NULL) {
        FreeMem(source_buffer, BENCH_BUFFER_BYTES);
        source_buffer = NULL;
    }
    if (timer_open) {
        CloseDevice((struct IORequest *)&timer_request);
        TimerBase = NULL;
    }
    if (failure != NULL) {
        return report_failure(failure);
    }
    progress_phase = PHASE_COMPLETE;
    if (!write_text(Output(), "result=pass\n")) {
        return RETURN_ERROR;
    }
    return RETURN_OK;
}
