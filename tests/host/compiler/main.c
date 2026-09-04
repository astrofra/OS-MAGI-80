#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "compiler/backend_m68k/backend.h"
#include "compiler/frontend/frontend.h"
#include "compiler/ir/ir.h"

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

struct evaluation_case {
    uint32_t arguments[3];
    uint32_t expected;
};

static int compile_source(const char *source, struct miga80_ir_function *ir,
                          struct miga80_diagnostic *diagnostic)
{
    struct miga80_ast_function ast;

    return miga80_parse_function(source, strlen(source), &ast, diagnostic) &&
           miga80_lower_function(&ast, ir, diagnostic);
}

static int test_valid_function(void)
{
    static const char source[] =
        "-- precedence, parameter reuse, and unary negation\n"
        "function arithmetic(a: i32, b: i32, c: i32): i32\n"
        "  return (a * 3 + b) - (a + -5) + c * 2\n"
        "end\n";
    static const struct evaluation_case cases[] = {
        {{0U, 0U, 0U}, 5U},
        {{7U, 5U, 2U}, 28U},
        {{UINT32_C(0xfffffffc), 1U, 3U}, 4U},
        {{3U, UINT32_C(0xfffffff9), UINT32_C(0xfffffffe)}, 0U},
        {{UINT32_C(0x7fffffff), 4U, 5U}, 17U},
        {{UINT32_C(0x80000000), UINT32_C(0xffffffff),
          UINT32_C(0xfffffffd)},
         UINT32_C(0xfffffffe)}
    };
    struct miga80_ir_function ir;
    struct miga80_diagnostic diagnostic;
    size_t index;
    FILE *assembly;
    char assembly_prefix[256];
    size_t assembly_prefix_size;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) ||
        strcmp(ir.name, "arithmetic") != 0 || ir.parameter_count != 3U) {
        return 0;
    }
    for (index = 0; index < ARRAY_COUNT(cases); ++index) {
        uint32_t result;

        if (!miga80_evaluate_ir(&ir, cases[index].arguments, 3U, &result,
                                &diagnostic) ||
            result != cases[index].expected) {
            return 0;
        }
    }

    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k(assembly, &ir, &diagnostic);
    rewind(assembly);
    assembly_prefix_size =
        fread(assembly_prefix, 1U, sizeof(assembly_prefix) - 1U, assembly);
    assembly_prefix[assembly_prefix_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_prefix, "native ABI 0.1") == NULL ||
        strstr(assembly_prefix, "move.l  %d0,-4(%a6)") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }
    return 1;
}

static int expect_error(const char *source, unsigned int line,
                        unsigned int column, const char *message)
{
    struct miga80_ast_function ast;
    struct miga80_diagnostic diagnostic;

    if (miga80_parse_function(source, strlen(source), &ast, &diagnostic)) {
        return 0;
    }
    return diagnostic.line == line && diagnostic.column == column &&
           strstr(diagnostic.message, message) != NULL;
}

int main(void)
{
    if (!test_valid_function() ||
        !expect_error("function f(a): i32 return a end", 1U, 13U,
                      "expected ':'") ||
        !expect_error("function f(a: i32): i32 return missing end", 1U, 32U,
                      "unknown identifier") ||
        !expect_error("function f(): i32 return 2147483648 end", 1U, 26U,
                      "out of range") ||
        !expect_error("function f(): i32 return 999999999999999999999 end",
                      1U, 26U, "out of range") ||
        !expect_error("function f(a: i32, a: i32): i32 return a end", 1U,
                      20U, "duplicate parameter") ||
        !expect_error("function f(a: i32, b: i32, c: i32, d: i32): i32 "
                      "return a end",
                      1U, 36U, "at most 3 parameters")) {
        fprintf(stderr, "compiler frontend/IR regression failed\n");
        return 1;
    }

    printf("PASS  compiler frontend, typed i32 IR, and GNU m68k renderer\n");
    return 0;
}
