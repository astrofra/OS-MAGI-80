#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler/backend_m68k/backend.h"
#include "compiler/frontend/frontend.h"
#include "compiler/ir/ir.h"
#include "compiler/value_ir/value_ir.h"

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

static void set_value(struct miga80_value_function *function,
                      unsigned int index, enum miga80_value_opcode opcode,
                      unsigned int left, unsigned int right,
                      unsigned int parameter_index)
{
    struct miga80_value_instruction *value = &function->values[index];

    (void)memset(value, 0, sizeof(*value));
    value->type = MIGA80_TYPE_I32;
    value->opcode = opcode;
    value->left = left;
    value->right = right;
    value->parameter_index = parameter_index;
    value->line = 1U;
    value->column = index + 1U;
    value->live = 1;
}

static void build_spill_fixture(struct miga80_value_function *function)
{
    unsigned int index;

    (void)memset(function, 0, sizeof(*function));
    (void)snprintf(function->name, sizeof(function->name), "spill_fixture");
    function->parameter_count = 3U;
    function->parameter_types[0] = MIGA80_TYPE_I32;
    function->parameter_types[1] = MIGA80_TYPE_I32;
    function->parameter_types[2] = MIGA80_TYPE_I32;
    function->result_type = MIGA80_TYPE_I32;
    for (index = 0U; index < 3U; ++index) {
        set_value(function, index, MIGA80_VALUE_PARAMETER,
                  MIGA80_INVALID_VALUE, MIGA80_INVALID_VALUE, index);
    }
    for (index = 3U; index < 7U; ++index) {
        set_value(function, index, MIGA80_VALUE_MUL, 0U, 1U, 0U);
    }
    set_value(function, 7U, MIGA80_VALUE_ADD, 0U, 1U, 0U);
    set_value(function, 8U, MIGA80_VALUE_SUB, 0U, 1U, 0U);
    set_value(function, 9U, MIGA80_VALUE_NEG, 0U, MIGA80_INVALID_VALUE, 0U);
    set_value(function, 10U, MIGA80_VALUE_MUL, 0U, 1U, 0U);
    set_value(function, 11U, MIGA80_VALUE_ADD, 3U, 4U, 0U);
    set_value(function, 12U, MIGA80_VALUE_ADD, 5U, 6U, 0U);
    set_value(function, 13U, MIGA80_VALUE_ADD, 7U, 8U, 0U);
    set_value(function, 14U, MIGA80_VALUE_ADD, 9U, 10U, 0U);
    set_value(function, 15U, MIGA80_VALUE_ADD, 11U, 12U, 0U);
    set_value(function, 16U, MIGA80_VALUE_ADD, 13U, 14U, 0U);
    set_value(function, 17U, MIGA80_VALUE_ADD, 15U, 16U, 0U);
    set_value(function, 18U, MIGA80_VALUE_ADD, 17U, 2U, 0U);
    function->value_count = 19U;
    function->result = 18U;
    function->block_count = 1U;
    function->entry_block = 0U;
    function->block_order_count = 1U;
    function->block_order[0] = 0U;
    function->blocks[0].first_value = 0U;
    function->blocks[0].value_count = function->value_count;
    function->blocks[0].predecessors[0] = MIGA80_INVALID_BLOCK;
    function->blocks[0].predecessors[1] = MIGA80_INVALID_BLOCK;
    function->blocks[0].successors[0] = MIGA80_INVALID_BLOCK;
    function->blocks[0].successors[1] = MIGA80_INVALID_BLOCK;
    function->blocks[0].condition = MIGA80_INVALID_VALUE;
    function->blocks[0].terminator = MIGA80_VALUE_RETURN;
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
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    size_t index;
    FILE *assembly;
    char assembly_prefix[2048];
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
        strstr(assembly_prefix, "native ABI 0.2") == NULL ||
        strstr(assembly_prefix, "move.l  %d0,-4(%a6)") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }

    if (!miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        return 0;
    }
    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_prefix_size =
        fread(assembly_prefix, 1U, sizeof(assembly_prefix) - 1U, assembly);
    assembly_prefix[assembly_prefix_size] = '\0';
    if (ferror(assembly) || strstr(assembly_prefix, "miga80c -O1") == NULL ||
        strstr(assembly_prefix, "link.w") != NULL ||
        strstr(assembly_prefix, "move.l  (%a7)+,%d") != NULL ||
        strstr(assembly_prefix, "muls.l") != NULL) {
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
        fprintf(stderr, "expected parser error, source was accepted: %s\n",
                source);
        return 0;
    }
    if (diagnostic.line != line || diagnostic.column != column ||
        strstr(diagnostic.message, message) == NULL) {
        fprintf(stderr,
                "parser error mismatch: expected %u:%u %s, got %u:%u %s\n",
                line, column, message, diagnostic.line, diagnostic.column,
                diagnostic.message);
        return 0;
    }
    return 1;
}

static int test_constant_folding(void)
{
    static const char source[] =
        "function folded(): i32 return -(2 + 3) * 4 + 0 end";
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[512];
    size_t assembly_size;
    unsigned int live_count = 0U;
    unsigned int index;
    FILE *assembly;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        value_ir.result >= value_ir.value_count ||
        value_ir.values[value_ir.result].opcode != MIGA80_VALUE_CONSTANT ||
        value_ir.values[value_ir.result].immediate != UINT32_C(0xffffffec)) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live) {
            ++live_count;
        }
    }
    if (live_count != 1U) {
        return 0;
    }

    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "moveq   #-20,%d0") == NULL ||
        strstr(assembly_text, "movem.l") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    return emitted && closed == 0;
}

static int test_signed_division_and_fault_liveness(void)
{
    static const char source[] =
        "function divide(a: i32, b: i32, c: i32): i32\n"
        "  local quotient: i32 = a / b\n"
        "  quotient /= c\n"
        "  local discarded: i32 = a / b\n"
        "  discarded = 0\n"
        "  return quotient\n"
        "end\n";
    static const char optimized_source[] =
        "function divided(a: i32): i32 return 35 / 5 + a / -1 end";
    static const char constant_divisor_source[] =
        "function halved(a: i32): i32 return a / 2 end";
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    const uint32_t arguments[] = {35U, 5U, 2U};
    char assembly_text[8192];
    uint32_t result;
    unsigned int live_divisions = 0U;
    unsigned int index;
    size_t assembly_size;
    FILE *assembly;
    int emitted;
    int closed;

    if (!miga80_divide_i32(35U, 5U, &result) || result != 7U ||
        !miga80_divide_i32(UINT32_C(0xffffffdd), 5U, &result) ||
        result != UINT32_C(0xfffffff9) ||
        !miga80_divide_i32(35U, UINT32_C(0xfffffffb), &result) ||
        result != UINT32_C(0xfffffff9) ||
        !miga80_divide_i32(UINT32_C(0xffffffdd),
                           UINT32_C(0xfffffffb), &result) ||
        result != 7U ||
        !miga80_divide_i32(UINT32_C(0x80000000), UINT32_MAX, &result) ||
        result != UINT32_C(0x80000000) ||
        miga80_divide_i32(1U, 0U, &result) ||
        !compile_source(source, &ir, &diagnostic) ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 3U ||
        miga80_evaluate_ir(&ir, (const uint32_t[]){7U, 0U, 1U}, 3U,
                           &result, &diagnostic) ||
        diagnostic.line != 2U || diagnostic.column != 27U ||
        strstr(diagnostic.message, "division by zero") == NULL ||
        miga80_evaluate_ir(&ir, (const uint32_t[]){7U, 1U, 0U}, 3U,
                           &result, &diagnostic) ||
        diagnostic.line != 3U || diagnostic.column != 12U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live &&
            value_ir.values[index].opcode == MIGA80_VALUE_DIV) {
            ++live_divisions;
        }
    }
    if (live_divisions != 3U || (assembly = tmpfile()) == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) || strstr(assembly_text, "divs.l") == NULL ||
        strstr(assembly_text, "fault_divzero") == NULL ||
        strstr(assembly_text, "movea.l 0(%a5),%a0") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0 ||
        !compile_source(optimized_source, &ir, &diagnostic) ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        !miga80_evaluate_ir(&ir, (const uint32_t[]){3U}, 1U, &result,
                            &diagnostic) ||
        result != 4U || (assembly = tmpfile()) == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) || strstr(assembly_text, "divs.l") != NULL ||
        strstr(assembly_text, "fault_divzero") != NULL ||
        strstr(assembly_text, "neg.l") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0 ||
        !compile_source(constant_divisor_source, &ir, &diagnostic) ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        !miga80_evaluate_ir(&ir, (const uint32_t[]){UINT32_C(0xfffffffd)},
                            1U, &result, &diagnostic) ||
        result != UINT32_MAX || (assembly = tmpfile()) == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "divs.l  #0x00000002") == NULL ||
        strstr(assembly_text, "fault_divzero") != NULL ||
        strstr(assembly_text, "tst.l") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    return emitted && closed == 0;
}

static int test_locals_and_entry_block(void)
{
    static const char source[] =
        "function locals(a: i32, b: i32, c: i32): i32 "
        "local x: i32 = a * 3 + b "
        "local y: i32 = x + c "
        "x = y - a y = x * 2 x = 12345 return y + c end";
    static const uint32_t arguments[] = {7U, 5U, 2U};
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[4096];
    unsigned int stores = 0U;
    unsigned int local_loads = 0U;
    unsigned int index;
    uint32_t result;
    size_t assembly_size;
    FILE *assembly;
    int dead_assignment_found = 0;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) || ir.local_count != 2U ||
        ir.block_count != 1U || ir.entry_block != 0U ||
        ir.blocks[0].first_instruction != 0U ||
        ir.blocks[0].instruction_count != ir.instruction_count ||
        ir.blocks[0].successor_count != 0U ||
        ir.blocks[0].successors[0] != MIGA80_INVALID_BLOCK ||
        ir.blocks[0].successors[1] != MIGA80_INVALID_BLOCK ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 44U) {
        return 0;
    }
    for (index = 0U; index < ir.instruction_count; ++index) {
        if (ir.instructions[index].opcode == MIGA80_IR_STORE_LOCAL_I32) {
            ++stores;
        } else if (ir.instructions[index].opcode ==
                   MIGA80_IR_PUSH_LOCAL_I32) {
            ++local_loads;
        }
    }
    if (stores != 5U || local_loads != 4U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].opcode == MIGA80_VALUE_CONSTANT &&
            value_ir.values[index].immediate == 12345U) {
            dead_assignment_found = !value_ir.values[index].live;
        }
    }
    if (!dead_assignment_found) {
        return 0;
    }

    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k(assembly, &ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "link.w  %a6,#-20") == NULL ||
        strstr(assembly_text, "#0x00003039") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }

    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) || strstr(assembly_text, "#0x00003039") != NULL ||
        strstr(assembly_text, "link.w") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }

    ir.block_count = 2U;
    if (miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                           &diagnostic) ||
        strstr(diagnostic.message, "basic block") == NULL) {
        return 0;
    }
    return !miga80_build_value_ir(&ir, &value_ir, &diagnostic) &&
           strstr(diagnostic.message, "basic block") != NULL;
}

static int test_bool_comparisons_and_cfg(void)
{
    static const char source[] =
        "function choose(a: i32, b: i32, flag: bool): i32 "
        "local x: i32 = a "
        "if flag != false then x = a + 1 else x = b - 1 end "
        "return x end";
    static const uint32_t true_arguments[] = {7U, 20U, 1U};
    static const uint32_t false_arguments[] = {7U, 20U, 0U};
    static const char bool_source[] =
        "function ordered(a: i32, b: i32): bool return a < b end";
    static const char coalesced_source[] =
        "function coalesced(a: i32, b: i32, flag: bool): i32 "
        "local x: i32 = a "
        "if flag then x = a + 1 else x = b - 1 end "
        "if flag then x = x + 2 else x = x - 3 end "
        "return x end";
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[8192];
    unsigned int phi_count = 0U;
    unsigned int index;
    uint32_t result;
    size_t assembly_size;
    FILE *assembly;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) ||
        ir.parameter_types[2] != MIGA80_TYPE_BOOL || ir.block_count != 4U ||
        ir.blocks[0].successor_count != 2U ||
        ir.blocks[1].successor_count != 1U ||
        ir.blocks[2].successor_count != 1U ||
        ir.blocks[3].successor_count != 0U ||
        !miga80_evaluate_ir(&ir, true_arguments,
                            ARRAY_COUNT(true_arguments), &result,
                            &diagnostic) ||
        result != 8U ||
        !miga80_evaluate_ir(&ir, false_arguments,
                            ARRAY_COUNT(false_arguments), &result,
                            &diagnostic) ||
        result != 19U ||
        miga80_evaluate_ir(&ir,
                           (const uint32_t[]){7U, 20U, 2U}, 3U, &result,
                           &diagnostic) ||
        strstr(diagnostic.message, "canonical") == NULL ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        value_ir.block_count != 4U) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live &&
            value_ir.values[index].opcode == MIGA80_VALUE_PHI) {
            ++phi_count;
        }
    }
    if (phi_count != 1U) {
        return 0;
    }

    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) || strstr(assembly_text, "sne") == NULL ||
        strstr(assembly_text, "beq") == NULL ||
        strstr(assembly_text, ".L_choose_b3:") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }
    ir.blocks[0].successors[0] = ir.blocks[0].successors[1];
    if (miga80_validate_ir(&ir, &diagnostic) ||
        strstr(diagnostic.message, "fallthrough") == NULL) {
        return 0;
    }
    if (!compile_source(bool_source, &ir, &diagnostic) ||
        ir.result_type != MIGA80_TYPE_BOOL ||
        !miga80_evaluate_ir(&ir, (const uint32_t[]){UINT32_C(0xffffffff), 0U},
                            2U, &result, &diagnostic) ||
        result != 1U) {
        return 0;
    }
    if (!compile_source(coalesced_source, &ir, &diagnostic) ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        return 0;
    }
    phi_count = 0U;
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live &&
            value_ir.values[index].opcode == MIGA80_VALUE_PHI) {
            ++phi_count;
        }
    }
    if (phi_count != 2U || (assembly = tmpfile()) == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "link.w  %a6,#-4") == NULL ||
        strstr(assembly_text, "link.w  %a6,#-8") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    return emitted && closed == 0;
}

static int test_while_cfg_and_loop_phis(void)
{
    static const char source[] =
        "function loops(a: i32, b: i32, c: i32): i32 "
        "local i: i32 = 0 local x: i32 = a local y: i32 = b "
        "local temp: i32 = 0 local total: i32 = c "
        "while i < 5 do temp = x x = y y = temp "
        "if i != 2 then total = total + x + i "
        "else total = total - y end i = i + 1 end "
        "return total + x * 3 - y end";
    static const uint32_t arguments[] = {7U, 5U, 2U};
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[8192];
    unsigned int live_phis = 0U;
    unsigned int forward_phis = 0U;
    unsigned int index;
    uint32_t result = 0U;
    size_t assembly_size;
    FILE *assembly;
    int emitted;
    int closed;

    (void)memset(&ir, 0, sizeof(ir));
    (void)memset(&value_ir, 0, sizeof(value_ir));
    (void)memset(&diagnostic, 0, sizeof(diagnostic));
    if (!compile_source(source, &ir, &diagnostic) || ir.block_count != 8U ||
        ir.blocks[0].successor_count != 1U ||
        ir.blocks[0].successors[0] != 1U ||
        ir.blocks[1].successor_count != 2U ||
        ir.blocks[1].successors[0] != 2U ||
        ir.blocks[1].successors[1] != 7U ||
        ir.blocks[6].instruction_count != 1U ||
        ir.instructions[ir.blocks[6].first_instruction].opcode !=
            MIGA80_IR_JUMP ||
        ir.blocks[6].successor_count != 1U ||
        ir.blocks[6].successors[0] != 1U ||
        ir.blocks[5].successor_count != 1U ||
        ir.blocks[5].successors[0] != 6U ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 35U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        value_ir.blocks[6].predecessor_count != 1U ||
        value_ir.blocks[6].predecessors[0] != 5U ||
        value_ir.blocks[7].predecessor_count != 1U ||
        value_ir.blocks[7].predecessors[0] != 1U ||
        value_ir.block_order_count != 8U ||
        value_ir.block_order[0] != 0U || value_ir.block_order[1] != 1U ||
        value_ir.block_order[2] != 2U || value_ir.block_order[3] != 3U ||
        value_ir.block_order[4] != 4U || value_ir.block_order[5] != 5U ||
        value_ir.block_order[6] != 6U || value_ir.block_order[7] != 7U) {
        fprintf(stderr,
                "while CFG mismatch: blocks=%u "
                "order=%u,%u,%u,%u,%u,%u,%u,%u "
                "result=%08x diagnostic=%s\n",
                ir.block_count, value_ir.block_order[0],
                value_ir.block_order[1], value_ir.block_order[2],
                value_ir.block_order[3], value_ir.block_order[4],
                value_ir.block_order[5], value_ir.block_order[6],
                value_ir.block_order[7],
                (unsigned int)result, diagnostic.message);
        return 0;
    }
    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k(assembly, &ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "bra     .L_loops_b1") == NULL ||
        strstr(assembly_text, "bra     .L_loops_b6") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live &&
            value_ir.values[index].opcode == MIGA80_VALUE_PHI) {
            ++live_phis;
            if (value_ir.values[index].right > index) {
                ++forward_phis;
            }
        }
    }
    if (live_phis != 5U || forward_phis != 3U ||
        (assembly = tmpfile()) == NULL) {
        fprintf(stderr, "while phi mismatch: live=%u forward=%u\n",
                live_phis, forward_phis);
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "link.w  %a6,#-20") == NULL ||
        strstr(assembly_text, "move.l  %d7,-20(%a6)") == NULL ||
        strstr(assembly_text, "bra     .L_loops_b1") == NULL ||
        strstr(assembly_text, "bra     .L_loops_b6") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    return emitted && closed == 0;
}

static int test_nested_while_and_trivial_phi(void)
{
    static const char source[] =
        "function nested(a: i32, b: i32, c: i32): i32 "
        "local outer: i32 = 0 local inner: i32 = 0 "
        "local total: i32 = a local stable: i32 = b "
        "while outer < 3 do stable = stable inner = 0 "
        "while inner < 2 do total = total + stable + c "
        "inner = inner + 1 end outer = outer + 1 end "
        "return total + stable end";
    static const uint32_t arguments[] = {7U, 5U, 2U};
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    uint32_t result;
    unsigned int index;
    unsigned int live_phis = 0U;
    FILE *assembly;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) || ir.block_count != 9U ||
        ir.blocks[7].instruction_count != 1U ||
        ir.blocks[7].successors[0] != 1U ||
        ir.blocks[5].instruction_count != 1U ||
        ir.blocks[5].successors[0] != 3U ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 54U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        return 0;
    }
    for (index = 0U; index < value_ir.value_count; ++index) {
        if (value_ir.values[index].live &&
            value_ir.values[index].opcode == MIGA80_VALUE_PHI) {
            ++live_phis;
        }
    }
    assembly = tmpfile();
    if (live_phis != 4U || assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    closed = fclose(assembly);
    return emitted && closed == 0;
}

static int test_break_continue_funnels(void)
{
    static const char source[] =
        "function loop_control(a: i32, b: i32, c: i32): i32 "
        "local i: i32 = 0 local total: i32 = a while i < 8 do "
        "i = i + 1 "
        "if i == 2 then continue else total = total + i end "
        "if i == b then break else total = total + c end "
        "if i == 5 then continue else total = total + 1 end "
        "if total > 100 then break else total = total end "
        "end return total + i end";
    static const char nested_source[] =
        "function nested_control(a: i32, b: i32, c: i32): i32 "
        "local outer: i32 = 0 local inner: i32 = 0 "
        "local total: i32 = a while outer < 3 do "
        "outer = outer + 1 inner = 0 while inner < 4 do "
        "inner = inner + 1 "
        "if inner == 2 then continue else total = total + 1 end "
        "if outer == 2 then break else total = total + c end end "
        "if outer == 3 then break else total = total + b end end "
        "return total + outer * 10 + inner end";
    static const char one_shot_source[] =
        "function one_shot(a: i32, b: i32, c: i32): i32 "
        "local total: i32 = a while true do total = total + b break end "
        "return total + c end";
    static const uint32_t arguments[] = {7U, 5U, 2U};
    struct miga80_ir_function ir;
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[16384];
    uint32_t result = 0U;
    size_t assembly_size;
    unsigned int block;
    FILE *assembly;
    int emitted;
    int closed;

    if (!compile_source(source, &ir, &diagnostic) || ir.block_count != 21U ||
        ir.blocks[15].instruction_count != 1U ||
        ir.blocks[15].successors[0] != 1U ||
        ir.blocks[17].successors[0] != 18U ||
        ir.blocks[18].successors[0] != 15U ||
        ir.blocks[19].successors[0] != 20U ||
        ir.blocks[20].successors[0] != 16U ||
        (ir.block_loop_membership[16] & (UINT32_C(1) << 1U)) != 0U ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 34U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic) ||
        value_ir.blocks[15].predecessor_count != 1U ||
        value_ir.blocks[15].predecessors[0] != 18U ||
        value_ir.blocks[16].predecessor_count != 1U ||
        value_ir.blocks[16].predecessors[0] != 20U) {
        fprintf(stderr, "loop-control CFG mismatch: blocks=%u result=%08x "
                        "diagnostic=%s\n",
                ir.block_count, (unsigned int)result, diagnostic.message);
        return 0;
    }
    for (block = 0U; block < value_ir.block_count; ++block) {
        if (value_ir.blocks[block].predecessor_count > 2U) {
            return 0;
        }
    }
    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "bra     .L_loop_control_b1") == NULL ||
        strstr(assembly_text, "bra     .L_loop_control_b15") != NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    if (!emitted || closed != 0 ||
        !compile_source(nested_source, &ir, &diagnostic) ||
        !miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments), &result,
                            &diagnostic) ||
        result != 70U ||
        !miga80_build_value_ir(&ir, &value_ir, &diagnostic)) {
        fprintf(stderr, "nested loop-control mismatch: result=%08x "
                        "diagnostic=%s\n",
                (unsigned int)result, diagnostic.message);
        return 0;
    }
    return compile_source(one_shot_source, &ir, &diagnostic) &&
           miga80_evaluate_ir(&ir, arguments, ARRAY_COUNT(arguments),
                              &result, &diagnostic) &&
           result == 14U &&
           miga80_build_value_ir(&ir, &value_ir, &diagnostic);
}

static int test_while_evaluation_budget(void)
{
    static const char source[] =
        "function spin(): i32 while true do end return 0 end";
    struct miga80_ir_function ir;
    struct miga80_diagnostic diagnostic;
    uint32_t result;

    return compile_source(source, &ir, &diagnostic) &&
           !miga80_evaluate_ir(&ir, NULL, 0U, &result, &diagnostic) &&
           strcmp(diagnostic.message,
                  "typed IR control-flow budget exceeded") == 0;
}

static int test_spill_frame(void)
{
    struct miga80_value_function value_ir;
    struct miga80_diagnostic diagnostic;
    char assembly_text[4096];
    size_t assembly_size;
    FILE *assembly;
    int emitted;
    int closed;

    build_spill_fixture(&value_ir);
    assembly = tmpfile();
    if (assembly == NULL) {
        return 0;
    }
    emitted = miga80_emit_gnu_m68k_o1(assembly, &value_ir, &diagnostic);
    rewind(assembly);
    assembly_size =
        fread(assembly_text, 1U, sizeof(assembly_text) - 1U, assembly);
    assembly_text[assembly_size] = '\0';
    if (ferror(assembly) ||
        strstr(assembly_text, "link.w  %a6,#-12") == NULL ||
        strstr(assembly_text, "%d3/%d4/%d5/%d6/%d7") == NULL ||
        strstr(assembly_text, "move.l  %d7,-") == NULL ||
        strstr(assembly_text, "(%a6),%d") == NULL ||
        strstr(assembly_text, "unlk    %a6") == NULL) {
        emitted = 0;
    }
    closed = fclose(assembly);
    return emitted && closed == 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--emit-spill-fixture") == 0) {
        struct miga80_value_function *value_ir;
        struct miga80_diagnostic diagnostic;
        int emitted;

        value_ir =
            (struct miga80_value_function *)malloc(sizeof(*value_ir));
        if (value_ir == NULL) {
            fprintf(stderr, "unable to allocate spill fixture IR\n");
            return 1;
        }
        build_spill_fixture(value_ir);
        emitted = miga80_emit_gnu_m68k_o1(stdout, value_ir, &diagnostic);
        free(value_ir);
        if (!emitted) {
            fprintf(stderr, "spill fixture emission failed: %s\n",
                    diagnostic.message);
            return 1;
        }
        return 0;
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--emit-spill-fixture]\n", argv[0]);
        return 2;
    }

    if (!test_valid_function() || !test_constant_folding() ||
        !test_signed_division_and_fault_liveness() ||
        !test_locals_and_entry_block() ||
        !test_bool_comparisons_and_cfg() ||
        !test_while_cfg_and_loop_phis() ||
        !test_nested_while_and_trivial_phi() ||
        !test_break_continue_funnels() ||
        !test_while_evaluation_budget() ||
        !test_spill_frame() ||
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
                      1U, 36U, "at most 3 parameters") ||
        !expect_error("function f(a: i32): i32 local a: i32 = 1 return a end",
                      1U, 31U, "duplicate local") ||
        !expect_error("function f(): i32 local x: i32 = 1 local x: i32 = 2 "
                      "return x end",
                      1U, 42U, "duplicate local") ||
        !expect_error("function f(a: i32): i32 a = 1 return a end", 1U,
                      25U, "cannot assign to parameter") ||
        !expect_error("function f(): i32 x = 1 return x end", 1U, 19U,
                      "unknown local assignment target") ||
        !expect_error("function f(): i32 local x: i32 = x return x end", 1U,
                      34U, "unknown identifier") ||
        !expect_error("function f(): i32 local x: bool = 1 return 0 end", 1U,
                      25U, "local initializer requires bool") ||
        !expect_error("function f(): bool return 1 end", 1U, 20U,
                      "function return requires bool") ||
        !expect_error("function f(a: i32): i32 if a then else end return a end",
                      1U, 25U, "if condition requires bool") ||
        !expect_error("function f(): i32 while 1 do end return 0 end", 1U,
                      19U, "while condition requires bool") ||
        !expect_error("function f(a: i32): i32 return a / (2 - 2) end", 1U,
                      34U, "division by zero in constant expression") ||
        !expect_error("function f(): i32 local x: bool = true x /= 1 "
                      "return 0 end",
                      1U, 42U, "operator '/=' requires i32 target") ||
        !expect_error("function f(flag: bool): i32 if flag /= true then "
                      "else end return 0 end",
                      1U, 37U, "expected 'then', found '/='") ||
        !expect_error("function f(): i32 break return 0 end", 1U, 19U,
                      "break is only valid inside while") ||
        !expect_error("function f(): i32 continue return 0 end", 1U, 19U,
                      "continue is only valid inside while") ||
        !expect_error("function f(): i32 while true do break x = 1 end "
                      "return 0 end",
                      1U, 39U, "statement follows loop control") ||
        !expect_error("function f(): bool return true < false end", 1U, 32U,
                      "ordered comparison requires i32") ||
        !expect_error("function f(): bool return 1 == true end", 1U, 29U,
                      "different types") ||
        !expect_error("function f(): bool return !true end", 1U, 27U,
                      "expected '=' after '!'") ||
        !expect_error("function f(): i32 if true then local x: i32 = 1 else "
                      "end return 0 end",
                      1U, 32U, "local declarations inside if")) {
        fprintf(stderr, "compiler frontend/IR regression failed\n");
        return 1;
    }

    printf("PASS  compiler CFG, loop control, signed division faults, phis, "
           "O0/O1, and spills\n");
    return 0;
}
