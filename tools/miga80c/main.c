#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/backend_m68k/backend.h"
#include "compiler/frontend/frontend.h"
#include "compiler/ir/ir.h"

#define MIGA80_MAX_SOURCE_SIZE (64U * 1024U)

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s source.lua -S -o output.s\n"
            "       %s source.lua --eval [i32-argument ...]\n",
            program, program);
}

static int read_source(const char *path, char **source, size_t *source_size)
{
    FILE *input = fopen(path, "rb");
    long file_size;
    size_t read_size;
    int close_result;
    int read_error;

    if (input == NULL) {
        fprintf(stderr, "%s: unable to open source\n", path);
        return 0;
    }
    if (fseek(input, 0L, SEEK_END) != 0 ||
        (file_size = ftell(input)) < 0L ||
        (unsigned long)file_size > MIGA80_MAX_SOURCE_SIZE ||
        fseek(input, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "%s: source is unreadable or exceeds %u bytes\n",
                path, MIGA80_MAX_SOURCE_SIZE);
        (void)fclose(input);
        return 0;
    }

    *source = (char *)malloc((size_t)file_size + 1U);
    if (*source == NULL) {
        fprintf(stderr, "%s: unable to allocate source buffer\n", path);
        (void)fclose(input);
        return 0;
    }
    read_size = fread(*source, 1U, (size_t)file_size, input);
    read_error = ferror(input);
    close_result = fclose(input);
    if (read_size != (size_t)file_size || read_error || close_result != 0) {
        fprintf(stderr, "%s: unable to read source\n", path);
        free(*source);
        *source = NULL;
        return 0;
    }
    (*source)[read_size] = '\0';
    *source_size = read_size;
    return 1;
}

static int parse_argument(const char *text, uint32_t *value)
{
    char *end = NULL;
    long long parsed;

    errno = 0;
    parsed = strtoll(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < (long long)INT32_MIN || parsed > (long long)UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static void print_diagnostic(const char *path,
                             const struct miga80_diagnostic *diagnostic)
{
    if (diagnostic->line == 0U) {
        fprintf(stderr, "%s: error: %s\n", path, diagnostic->message);
    } else {
        fprintf(stderr, "%s:%u:%u: error: %s\n", path, diagnostic->line,
                diagnostic->column, diagnostic->message);
    }
}

static int write_assembly(const char *path,
                          const struct miga80_ir_function *function,
                          struct miga80_diagnostic *diagnostic)
{
    FILE *output = fopen(path, "wb");
    int success;

    if (output == NULL) {
        fprintf(stderr, "%s: unable to create assembly output\n", path);
        return 0;
    }
    success = miga80_emit_gnu_m68k(output, function, diagnostic);
    if (fclose(output) != 0) {
        success = 0;
        if (diagnostic->message[0] == '\0') {
            (void)snprintf(diagnostic->message,
                           sizeof(diagnostic->message),
                           "unable to close assembly output");
        }
    }
    return success;
}

int main(int argc, char **argv)
{
    char *source = NULL;
    size_t source_size = 0U;
    struct miga80_ast_function ast;
    struct miga80_ir_function ir;
    struct miga80_diagnostic diagnostic;
    int exit_code = 1;

    if (argc < 3) {
        usage(argv[0]);
        return 2;
    }
    if (!read_source(argv[1], &source, &source_size)) {
        return 1;
    }
    if (!miga80_parse_function(source, source_size, &ast, &diagnostic) ||
        !miga80_lower_function(&ast, &ir, &diagnostic)) {
        print_diagnostic(argv[1], &diagnostic);
        free(source);
        return 1;
    }

    if (argc == 5 && strcmp(argv[2], "-S") == 0 &&
        strcmp(argv[3], "-o") == 0) {
        if (!write_assembly(argv[4], &ir, &diagnostic)) {
            print_diagnostic(argv[4], &diagnostic);
        } else {
            exit_code = 0;
        }
    } else if (strcmp(argv[2], "--eval") == 0) {
        uint32_t arguments[MIGA80_MAX_PARAMETERS] = {0U, 0U, 0U};
        uint32_t result;
        unsigned int index;
        const unsigned int argument_count = (unsigned int)(argc - 3);

        if (argument_count != ir.parameter_count) {
            fprintf(stderr, "%s: expected %u argument(s), got %u\n", argv[1],
                    ir.parameter_count, argument_count);
        } else {
            for (index = 0; index < argument_count; ++index) {
                if (!parse_argument(argv[index + 3U], &arguments[index])) {
                    fprintf(stderr, "%s: invalid i32 argument '%s'\n",
                            argv[1], argv[index + 3U]);
                    break;
                }
            }
            if (index == argument_count &&
                miga80_evaluate_ir(&ir, arguments, argument_count, &result,
                                   &diagnostic)) {
                printf("0x%08x\n", (unsigned int)result);
                exit_code = 0;
            } else if (index == argument_count) {
                print_diagnostic(argv[1], &diagnostic);
            }
        }
    } else {
        usage(argv[0]);
        exit_code = 2;
    }

    free(source);
    return exit_code;
}
