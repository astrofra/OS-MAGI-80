#include "compiler/backend_m68k/backend.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "compiler/abi/abi.h"

#define MIGA80_DATA_REGISTER_COUNT 8U
#define MIGA80_NO_REGISTER (-1)

struct allocation_plan {
    int registers[MIGA80_MAX_VALUE_INSTRUCTIONS];
    unsigned int last_use[MIGA80_MAX_VALUE_INSTRUCTIONS];
    int saved_registers[MIGA80_DATA_REGISTER_COUNT];
};

static int output_line(FILE *output, const char *format, ...)
{
    int written;
    va_list arguments;

    va_start(arguments, format);
    written = vfprintf(output, format, arguments);
    va_end(arguments);
    return written >= 0;
}

static int fail(struct miga80_diagnostic *diagnostic, unsigned int line,
                unsigned int column, const char *message)
{
    diagnostic->line = line;
    diagnostic->column = column;
    (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s",
                   message);
    return 0;
}

static int opcode_has_left(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_NEG || opcode == MIGA80_VALUE_ADD ||
           opcode == MIGA80_VALUE_SUB || opcode == MIGA80_VALUE_MUL;
}

static int opcode_has_right(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_SUB ||
           opcode == MIGA80_VALUE_MUL;
}

static int opcode_is_commutative(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_MUL;
}

static int validate_value_function(
    const struct miga80_value_function *function,
    struct miga80_diagnostic *diagnostic)
{
    unsigned int index;

    if (function->value_count == 0U ||
        function->value_count > MIGA80_MAX_VALUE_INSTRUCTIONS ||
        function->result >= function->value_count ||
        !function->values[function->result].live) {
        return fail(diagnostic, 0U, 0U, "invalid O1 value function");
    }
    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (!value->live) {
            continue;
        }
        if (value->type != MIGA80_VALUE_I32 ||
            value->opcode < MIGA80_VALUE_CONSTANT ||
            value->opcode > MIGA80_VALUE_MUL) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 value instruction");
        }
        if ((opcode_has_left(value->opcode) && value->left >= index) ||
            (opcode_has_right(value->opcode) && value->right >= index)) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 value operand");
        }
        if (value->opcode == MIGA80_VALUE_PARAMETER &&
            value->parameter_index >= function->parameter_count) {
            return fail(diagnostic, value->line, value->column,
                        "invalid O1 parameter index");
        }
    }
    return 1;
}

static int find_free_register(const unsigned int *owners,
                              unsigned int preferred)
{
    unsigned int reg;

    if (preferred < MIGA80_DATA_REGISTER_COUNT &&
        owners[preferred] == MIGA80_INVALID_VALUE) {
        return (int)preferred;
    }
    for (reg = 0U; reg < MIGA80_DATA_REGISTER_COUNT; ++reg) {
        if (owners[reg] == MIGA80_INVALID_VALUE) {
            return (int)reg;
        }
    }
    return MIGA80_NO_REGISTER;
}

static int build_allocation_plan(const struct miga80_value_function *function,
                                 struct allocation_plan *plan,
                                 struct miga80_diagnostic *diagnostic)
{
    unsigned int owners[MIGA80_DATA_REGISTER_COUNT];
    unsigned int index;

    for (index = 0U; index < MIGA80_MAX_VALUE_INSTRUCTIONS; ++index) {
        plan->registers[index] = MIGA80_NO_REGISTER;
        plan->last_use[index] = index;
    }
    for (index = 0U; index < MIGA80_DATA_REGISTER_COUNT; ++index) {
        owners[index] = MIGA80_INVALID_VALUE;
        plan->saved_registers[index] = 0;
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (!value->live) {
            continue;
        }
        if (opcode_has_left(value->opcode)) {
            plan->last_use[value->left] = index;
        }
        if (opcode_has_right(value->opcode)) {
            plan->last_use[value->right] = index;
        }
    }
    plan->last_use[function->result] = function->value_count;

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (value->live && value->opcode == MIGA80_VALUE_PARAMETER) {
            enum miga80_abi_register abi_register;
            unsigned int reg;

            if (!miga80_abi_scalar_argument_register(value->parameter_index,
                                                       &abi_register) ||
                abi_register < MIGA80_ABI_D0 ||
                abi_register > MIGA80_ABI_D7) {
                return fail(diagnostic, value->line, value->column,
                            "O1 parameter register is invalid");
            }
            reg = (unsigned int)(abi_register - MIGA80_ABI_D0);
            if (owners[reg] != MIGA80_INVALID_VALUE) {
                return fail(diagnostic, value->line, value->column,
                            "O1 parameter register is assigned twice");
            }
            owners[reg] = index;
            plan->registers[index] = (int)reg;
        }
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];
        int left_register = MIGA80_NO_REGISTER;
        int right_register = MIGA80_NO_REGISTER;
        int destination = MIGA80_NO_REGISTER;
        unsigned int preferred = MIGA80_DATA_REGISTER_COUNT;

        if (!value->live || value->opcode == MIGA80_VALUE_CONSTANT ||
            value->opcode == MIGA80_VALUE_PARAMETER) {
            continue;
        }
        if (opcode_has_left(value->opcode)) {
            left_register = plan->registers[value->left];
        }
        if (opcode_has_right(value->opcode)) {
            right_register = plan->registers[value->right];
        }

        if (value->opcode == MIGA80_VALUE_MUL &&
            function->values[value->right].opcode == MIGA80_VALUE_CONSTANT &&
            function->values[value->right].immediate == 3U) {
            destination = find_free_register(owners, preferred);
        }
        if (destination == MIGA80_NO_REGISTER &&
            left_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->left] == index) {
            destination = left_register;
        }
        if (destination == MIGA80_NO_REGISTER &&
            opcode_is_commutative(value->opcode) &&
            right_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->right] == index) {
            destination = right_register;
        }
        if (function->result == index) {
            preferred = 0U;
        }
        if (destination == MIGA80_NO_REGISTER) {
            destination = find_free_register(owners, preferred);
        }
        if (destination == MIGA80_NO_REGISTER) {
            return fail(diagnostic, value->line, value->column,
                        "O1 data-register pressure exceeds bootstrap limit");
        }

        if (left_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->left] == index &&
            left_register != destination) {
            owners[left_register] = MIGA80_INVALID_VALUE;
        }
        if (right_register != MIGA80_NO_REGISTER &&
            plan->last_use[value->right] == index &&
            right_register != destination) {
            owners[right_register] = MIGA80_INVALID_VALUE;
        }
        owners[destination] = index;
        plan->registers[index] = destination;
        if (destination >= (int)(MIGA80_ABI_D3 - MIGA80_ABI_D0)) {
            plan->saved_registers[destination] = 1;
        }
    }
    return 1;
}

static const char *data_register_name(int reg)
{
    return miga80_abi_gnu_register_name(
        (enum miga80_abi_register)(MIGA80_ABI_D0 + reg));
}

static int emit_move(FILE *output,
                     const struct miga80_value_function *function,
                     const struct allocation_plan *plan, unsigned int source,
                     int destination)
{
    const struct miga80_value_instruction *value = &function->values[source];

    if (value->opcode == MIGA80_VALUE_CONSTANT) {
        const uint32_t constant = value->immediate;

        if (constant <= UINT32_C(127)) {
            return output_line(output, "        moveq   #%u,%s\n",
                               (unsigned int)constant,
                               data_register_name(destination));
        }
        if (constant >= UINT32_C(0xffffff80)) {
            return output_line(output, "        moveq   #-%u,%s\n",
                               (unsigned int)(0U - constant),
                               data_register_name(destination));
        }
        return output_line(output, "        move.l  #0x%08x,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (plan->registers[source] == destination) {
        return 1;
    }
    return output_line(output, "        move.l  %s,%s\n",
                       data_register_name(plan->registers[source]),
                       data_register_name(destination));
}

static int emit_add_immediate(FILE *output, uint32_t constant,
                              int destination)
{
    if (constant >= 1U && constant <= 8U) {
        return output_line(output, "        addq.l  #%u,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (constant >= UINT32_C(0xfffffff8)) {
        return output_line(output, "        subq.l  #%u,%s\n",
                           (unsigned int)(0U - constant),
                           data_register_name(destination));
    }
    return output_line(output, "        add.l   #0x%08x,%s\n",
                       (unsigned int)constant,
                       data_register_name(destination));
}

static int emit_sub_immediate(FILE *output, uint32_t constant,
                              int destination)
{
    if (constant >= 1U && constant <= 8U) {
        return output_line(output, "        subq.l  #%u,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    if (constant >= UINT32_C(0xfffffff8)) {
        return output_line(output, "        addq.l  #%u,%s\n",
                           (unsigned int)(0U - constant),
                           data_register_name(destination));
    }
    return output_line(output, "        sub.l   #0x%08x,%s\n",
                       (unsigned int)constant,
                       data_register_name(destination));
}

static unsigned int power_of_two_shift(uint32_t constant)
{
    unsigned int shift = 0U;

    if (constant == 0U || (constant & (constant - 1U)) != 0U) {
        return 0U;
    }
    while (constant > 1U) {
        constant >>= 1;
        ++shift;
    }
    return shift;
}

static int emit_multiply(FILE *output,
                         const struct miga80_value_function *function,
                         const struct allocation_plan *plan,
                         const struct miga80_value_instruction *value,
                         int destination, unsigned int source)
{
    const struct miga80_value_instruction *operand = &function->values[source];

    if (operand->opcode == MIGA80_VALUE_CONSTANT) {
        const uint32_t constant = operand->immediate;
        const unsigned int shift = power_of_two_shift(constant);

        if (constant == 2U) {
            return output_line(output, "        add.l   %s,%s\n",
                               data_register_name(destination),
                               data_register_name(destination));
        }
        if (constant == 3U &&
            destination != plan->registers[value->left]) {
            return output_line(output,
                               "        add.l   %s,%s\n"
                               "        add.l   %s,%s\n",
                               data_register_name(destination),
                               data_register_name(destination),
                               data_register_name(plan->registers[value->left]),
                               data_register_name(destination));
        }
        if (shift >= 1U && shift <= 8U) {
            return output_line(output, "        lsl.l   #%u,%s\n", shift,
                               data_register_name(destination));
        }
        return output_line(output, "        muls.l  #0x%08x,%s\n",
                           (unsigned int)constant,
                           data_register_name(destination));
    }
    return output_line(output, "        muls.l  %s,%s\n",
                       data_register_name(plan->registers[source]),
                       data_register_name(destination));
}

static int emit_value(FILE *output,
                      const struct miga80_value_function *function,
                      const struct allocation_plan *plan, unsigned int index)
{
    const struct miga80_value_instruction *value = &function->values[index];
    const int destination = plan->registers[index];
    unsigned int source = value->right;

    if (value->opcode == MIGA80_VALUE_NEG) {
        return emit_move(output, function, plan, value->left, destination) &&
               output_line(output, "        neg.l   %s\n",
                           data_register_name(destination));
    }

    if (opcode_is_commutative(value->opcode) &&
        plan->registers[value->right] == destination &&
        function->values[value->right].opcode != MIGA80_VALUE_CONSTANT) {
        source = value->left;
    } else if (!emit_move(output, function, plan, value->left, destination)) {
        return 0;
    }

    if (function->values[source].opcode == MIGA80_VALUE_CONSTANT) {
        if (value->opcode == MIGA80_VALUE_ADD) {
            return emit_add_immediate(output,
                                      function->values[source].immediate,
                                      destination);
        }
        if (value->opcode == MIGA80_VALUE_SUB) {
            return emit_sub_immediate(output,
                                      function->values[source].immediate,
                                      destination);
        }
    }
    if (value->opcode == MIGA80_VALUE_ADD) {
        return output_line(output, "        add.l   %s,%s\n",
                           data_register_name(plan->registers[source]),
                           data_register_name(destination));
    }
    if (value->opcode == MIGA80_VALUE_SUB) {
        return output_line(output, "        sub.l   %s,%s\n",
                           data_register_name(plan->registers[source]),
                           data_register_name(destination));
    }
    return emit_multiply(output, function, plan, value, destination, source);
}

static int build_saved_register_list(const struct allocation_plan *plan,
                                     char *text, size_t text_size)
{
    size_t used = 0U;
    unsigned int reg;

    text[0] = '\0';
    for (reg = 3U; reg < MIGA80_DATA_REGISTER_COUNT; ++reg) {
        int written;

        if (!plan->saved_registers[reg]) {
            continue;
        }
        written = snprintf(text + used, text_size - used, "%s%s",
                           used == 0U ? "" : "/", data_register_name((int)reg));
        if (written < 0 || (size_t)written >= text_size - used) {
            return 0;
        }
        used += (size_t)written;
    }
    return 1;
}

int miga80_emit_gnu_m68k_o1(FILE *output,
                            const struct miga80_value_function *function,
                            struct miga80_diagnostic *diagnostic)
{
    struct allocation_plan plan;
    char saved_registers[64];
    unsigned int index;

    if (output == NULL || function == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    if (!validate_value_function(function, diagnostic) ||
        !build_allocation_plan(function, &plan, diagnostic) ||
        !build_saved_register_list(&plan, saved_registers,
                                   sizeof(saved_registers))) {
        if (diagnostic->message[0] == '\0') {
            (void)fail(diagnostic, 0U, 0U,
                       "unable to build O1 saved-register set");
        }
        return 0;
    }

    if (!output_line(output,
                     "/* Generated by miga80c -O1 for native ABI %u.%u; "
                     "development oracle only. */\n"
                     "        .text\n"
                     "        .even\n"
                     "        .globl  %s\n"
                     "%s:\n",
                     MIGA80_ABI_VERSION_MAJOR, MIGA80_ABI_VERSION_MINOR,
                     function->name, function->name)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 assembly");
    }
    if (saved_registers[0] != '\0' &&
        !output_line(output, "        movem.l %s,-(%%a7)\n",
                     saved_registers)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 prologue");
    }

    for (index = 0U; index < function->value_count; ++index) {
        const struct miga80_value_instruction *value =
            &function->values[index];

        if (value->live && value->opcode != MIGA80_VALUE_CONSTANT &&
            value->opcode != MIGA80_VALUE_PARAMETER &&
            !emit_value(output, function, &plan, index)) {
            return fail(diagnostic, value->line, value->column,
                        "unable to write O1 value instruction");
        }
    }
    if (!emit_move(output, function, &plan, function->result, 0)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 result");
    }
    if (saved_registers[0] != '\0' &&
        !output_line(output, "        movem.l (%%a7)+,%s\n",
                     saved_registers)) {
        return fail(diagnostic, 0U, 0U, "unable to write O1 epilogue");
    }
    if (!output_line(output, "        rts\n") || fflush(output) != 0 ||
        ferror(output)) {
        return fail(diagnostic, 0U, 0U, "unable to finish O1 assembly");
    }
    return 1;
}
