#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dos/dos.h>
#include <exec/memory.h>
#include <proto/dos.h>
#include <proto/exec.h>

#ifndef MIGA80_RUNTIME_NAME
#error "MIGA80_RUNTIME_NAME must identify the selected C runtime"
#endif

static const char *stdio_failure = "stdio";

static int make_path(char *destination, size_t capacity, const char *base,
                     const char *leaf)
{
    size_t base_length = strlen(base);
    size_t leaf_length = strlen(leaf);
    int needs_separator = 0;

    if (base_length > 0U && base[base_length - 1U] != ':' &&
        base[base_length - 1U] != '/') {
        needs_separator = 1;
    }

    if (base_length + (size_t)needs_separator + leaf_length + 1U > capacity) {
        return 0;
    }

    memcpy(destination, base, base_length);
    if (needs_separator) {
        destination[base_length++] = '/';
    }
    memcpy(destination + base_length, leaf, leaf_length + 1U);
    return 1;
}

static int test_heap(const char **zero_result)
{
    uint8_t *memory = (uint8_t *)malloc(64U);
    uint8_t *resized;
    uint8_t *cleared;
    void *zero_allocation;
    volatile size_t zero_size = 0U;
    size_t index;

    if (memory == NULL) {
        return 0;
    }
    for (index = 0U; index < 64U; ++index) {
        memory[index] = (uint8_t)(index ^ 0x5aU);
    }

    resized = (uint8_t *)realloc(memory, 128U);
    if (resized == NULL) {
        free(memory);
        return 0;
    }
    for (index = 0U; index < 64U; ++index) {
        if (resized[index] != (uint8_t)(index ^ 0x5aU)) {
            free(resized);
            return 0;
        }
    }
    free(resized);

    cleared = (uint8_t *)calloc(16U, 4U);
    if (cleared == NULL) {
        return 0;
    }
    for (index = 0U; index < 64U; ++index) {
        if (cleared[index] != 0U) {
            free(cleared);
            return 0;
        }
    }
    free(cleared);

    zero_allocation = malloc(zero_size);
    if (zero_allocation == NULL) {
        *zero_result = "null";
    } else {
        *zero_result = "non_null";
        free(zero_allocation);
    }

    return 1;
}

static int test_stdio(const char *base)
{
    static const uint8_t payload[] = {
        0x4dU, 0x41U, 0x47U, 0x49U, 0x2dU, 0x38U, 0x30U, 0x00U,
        0xffU, 0x80U, 0x01U, 0x02U, 0x03U, 0x0aU, 0x0dU, 0x5aU
    };
    uint8_t input[sizeof(payload)];
    char first_path[256];
    char second_path[256];
    char missing_path[256];
    FILE *file;

    if (!make_path(first_path, sizeof(first_path), base, "runtime-test.tmp") ||
        !make_path(second_path, sizeof(second_path), base, "runtime-test.renamed") ||
        !make_path(missing_path, sizeof(missing_path), base, "runtime-test.missing")) {
        stdio_failure = "stdio_path";
        return 0;
    }

    (void)remove(first_path);
    (void)remove(second_path);

    file = fopen(first_path, "wb");
    if (file == NULL) {
        stdio_failure = "stdio_open_write";
        return 0;
    }
    if (fwrite(payload, 1U, sizeof(payload), file) != sizeof(payload) ||
        fflush(file) != 0 || ftell(file) != (long)sizeof(payload)) {
        (void)fclose(file);
        (void)remove(first_path);
        stdio_failure = "stdio_write_flush_tell";
        return 0;
    }
    if (fclose(file) != 0) {
        (void)remove(first_path);
        stdio_failure = "stdio_close_write";
        return 0;
    }

    file = fopen(first_path, "rb");
    if (file == NULL) {
        (void)remove(first_path);
        stdio_failure = "stdio_open_read";
        return 0;
    }
    if (fseek(file, 0L, SEEK_SET) != 0 ||
        fread(input, 1U, sizeof(input), file) != sizeof(input) ||
        memcmp(input, payload, sizeof(payload)) != 0) {
        (void)fclose(file);
        (void)remove(first_path);
        stdio_failure = "stdio_read_seek_compare";
        return 0;
    }
    if (fclose(file) != 0) {
        (void)remove(first_path);
        stdio_failure = "stdio_close_read";
        return 0;
    }

#ifndef MIGA80_NEWLIB_COMPAT
    if (rename(first_path, second_path) != 0 || remove(second_path) != 0) {
        (void)remove(first_path);
        (void)remove(second_path);
        stdio_failure = "stdio_rename_remove";
        return 0;
    }
#else
    if (remove(first_path) != 0) {
        stdio_failure = "stdio_remove";
        return 0;
    }
#endif

    errno = 0;
    file = fopen(missing_path, "rb");
    if (file != NULL
#ifndef MIGA80_NEWLIB_COMPAT
        || errno == 0
#endif
    ) {
        if (file != NULL) {
            (void)fclose(file);
        }
        stdio_failure = "stdio_missing_errno";
        return 0;
    }

    return 1;
}

static int test_amigaos(const char *base)
{
    BPTR lock = Lock(base, ACCESS_READ);
    uint8_t *chip_memory;
    size_t index;

    if (lock == (BPTR)0) {
        return 0;
    }
    UnLock(lock);

    chip_memory = (uint8_t *)AllocMem(64U, MEMF_CHIP | MEMF_CLEAR);
    if (chip_memory == NULL) {
        return 0;
    }
    for (index = 0U; index < 64U; ++index) {
        if (chip_memory[index] != 0U) {
            FreeMem(chip_memory, 64U);
            return 0;
        }
    }
    FreeMem(chip_memory, 64U);
    return 1;
}

static int fail(const char *stage)
{
    (void)printf("failure=%s\nresult=fail\n", stage);
    return RETURN_FAIL;
}

int main(int argc, char **argv)
{
    static const char dos_message[] = "dos_output=pass\n";
    const char *zero_result;
    BPTR output;

    if (argc != 2 || argv == NULL || argv[1] == NULL || argv[1][0] == '\0') {
        return fail("arguments");
    }
    if (!test_heap(&zero_result)) {
        return fail("heap");
    }
    if (!test_stdio(argv[1])) {
        return fail(stdio_failure);
    }
    if (!test_amigaos(argv[1])) {
        return fail("amigaos");
    }

    (void)printf("runtime=%s\n", MIGA80_RUNTIME_NAME);
    (void)printf("arguments=pass\n");
    (void)printf("heap=pass\n");
    (void)printf("malloc_zero=%s\n", zero_result);
    (void)printf("stdio_file=pass\n");
    (void)printf("stdio_missing_errno="
#ifndef MIGA80_NEWLIB_COMPAT
                 "pass\n");
#else
                 "unavailable\n");
#endif
#ifndef MIGA80_NEWLIB_COMPAT
    (void)printf("c_rename=pass\n");
#else
    (void)printf("c_rename=unavailable\n");
#endif
    (void)printf("dos_lock=pass\n");
    (void)printf("chip_memory=pass\n");
    if (fflush(stdout) != 0) {
        return fail("stdout_flush");
    }

    output = Output();
    if (output == (BPTR)0 ||
        Write(output, dos_message, (LONG)(sizeof(dos_message) - 1U)) !=
            (LONG)(sizeof(dos_message) - 1U)) {
        return fail("dos_output");
    }

    (void)printf("result=pass\n");
    if (fflush(stdout) != 0) {
        return RETURN_ERROR;
    }
    return RETURN_OK;
}
