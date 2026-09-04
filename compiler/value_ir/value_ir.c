#include "compiler/value_ir/value_ir.h"

#include <stdio.h>
#include <string.h>

static int fail(struct miga80_diagnostic *diagnostic, unsigned int line,
                unsigned int column, const char *message)
{
    diagnostic->line = line;
    diagnostic->column = column;
    (void)snprintf(diagnostic->message, sizeof(diagnostic->message), "%s",
                   message);
    return 0;
}

static unsigned int add_value(struct miga80_value_function *function,
                              enum miga80_value_opcode opcode,
                              unsigned int left, unsigned int right,
                              uint32_t immediate,
                              unsigned int parameter_index,
                              unsigned int line, unsigned int column,
                              struct miga80_diagnostic *diagnostic)
{
    struct miga80_value_instruction *value;
    const unsigned int index = function->value_count;

    if (index == MIGA80_MAX_VALUE_INSTRUCTIONS) {
        (void)fail(diagnostic, line, column,
                   "value IR instruction limit exceeded");
        return MIGA80_INVALID_VALUE;
    }
    value = &function->values[index];
    (void)memset(value, 0, sizeof(*value));
    value->type = MIGA80_VALUE_I32;
    value->opcode = opcode;
    value->left = left;
    value->right = right;
    value->immediate = immediate;
    value->parameter_index = parameter_index;
    value->line = line;
    value->column = column;
    ++function->value_count;
    return index;
}

static int is_constant(const struct miga80_value_function *function,
                       unsigned int index, uint32_t *constant)
{
    if (index >= function->value_count ||
        function->values[index].opcode != MIGA80_VALUE_CONSTANT) {
        return 0;
    }
    if (constant != NULL) {
        *constant = function->values[index].immediate;
    }
    return 1;
}

static unsigned int make_constant(struct miga80_value_function *function,
                                  uint32_t constant, unsigned int line,
                                  unsigned int column,
                                  struct miga80_diagnostic *diagnostic)
{
    return add_value(function, MIGA80_VALUE_CONSTANT, MIGA80_INVALID_VALUE,
                     MIGA80_INVALID_VALUE, constant, 0U, line, column,
                     diagnostic);
}

static unsigned int make_neg(struct miga80_value_function *function,
                             unsigned int operand, unsigned int line,
                             unsigned int column,
                             struct miga80_diagnostic *diagnostic)
{
    uint32_t constant;

    if (is_constant(function, operand, &constant)) {
        return make_constant(function, 0U - constant, line, column,
                             diagnostic);
    }
    if (function->values[operand].opcode == MIGA80_VALUE_NEG) {
        return function->values[operand].left;
    }
    return add_value(function, MIGA80_VALUE_NEG, operand,
                     MIGA80_INVALID_VALUE, 0U, 0U, line, column, diagnostic);
}

static unsigned int make_binary(struct miga80_value_function *function,
                                enum miga80_value_opcode opcode,
                                unsigned int left, unsigned int right,
                                unsigned int line, unsigned int column,
                                struct miga80_diagnostic *diagnostic)
{
    uint32_t left_constant = 0U;
    uint32_t right_constant = 0U;
    int left_is_constant = is_constant(function, left, &left_constant);
    int right_is_constant = is_constant(function, right, &right_constant);

    if ((opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_MUL) &&
        left_is_constant && !right_is_constant) {
        const unsigned int temporary = left;

        left = right;
        right = temporary;
        right_constant = left_constant;
        left_is_constant = 0;
        right_is_constant = 1;
    }

    if (left_is_constant && right_is_constant) {
        uint32_t folded;

        if (opcode == MIGA80_VALUE_ADD) {
            folded = left_constant + right_constant;
        } else if (opcode == MIGA80_VALUE_SUB) {
            folded = left_constant - right_constant;
        } else {
            folded = left_constant * right_constant;
        }
        return make_constant(function, folded, line, column, diagnostic);
    }

    if (opcode == MIGA80_VALUE_ADD && right_is_constant &&
        right_constant == 0U) {
        return left;
    }
    if (opcode == MIGA80_VALUE_SUB) {
        if (right_is_constant && right_constant == 0U) {
            return left;
        }
        if (left == right) {
            return make_constant(function, 0U, line, column, diagnostic);
        }
    }
    if (opcode == MIGA80_VALUE_MUL && right_is_constant) {
        if (right_constant == 0U) {
            return make_constant(function, 0U, line, column, diagnostic);
        }
        if (right_constant == 1U) {
            return left;
        }
    }

    return add_value(function, opcode, left, right, 0U, 0U, line, column,
                     diagnostic);
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

static int mark_live_values(struct miga80_value_function *function,
                            struct miga80_diagnostic *diagnostic)
{
    unsigned int remaining;

    if (function->result >= function->value_count) {
        return fail(diagnostic, 0U, 0U, "value IR result is invalid");
    }
    function->values[function->result].live = 1;
    remaining = function->value_count;
    while (remaining != 0U) {
        struct miga80_value_instruction *value;

        --remaining;
        value = &function->values[remaining];
        if (!value->live) {
            continue;
        }
        if (opcode_has_left(value->opcode)) {
            if (value->left >= remaining) {
                return fail(diagnostic, value->line, value->column,
                            "value IR left operand is invalid");
            }
            function->values[value->left].live = 1;
            ++function->values[value->left].use_count;
        }
        if (opcode_has_right(value->opcode)) {
            if (value->right >= remaining) {
                return fail(diagnostic, value->line, value->column,
                            "value IR right operand is invalid");
            }
            function->values[value->right].live = 1;
            ++function->values[value->right].use_count;
        }
    }
    ++function->values[function->result].use_count;
    return 1;
}

int miga80_build_value_ir(const struct miga80_ir_function *source,
                          struct miga80_value_function *result,
                          struct miga80_diagnostic *diagnostic)
{
    unsigned int stack[MIGA80_MAX_IR_STACK];
    unsigned int parameter_values[MIGA80_MAX_PARAMETERS];
    unsigned int local_values[MIGA80_MAX_LOCALS];
    unsigned int stack_size = 0U;
    unsigned int index;
    int returned = 0;

    if (source == NULL || result == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(result, 0, sizeof(*result));
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    (void)memcpy(result->name, source->name, sizeof(result->name));
    result->parameter_count = source->parameter_count;
    result->result = MIGA80_INVALID_VALUE;
    if (!miga80_validate_straight_line_ir(source, diagnostic)) {
        return 0;
    }
    for (index = 0; index < MIGA80_MAX_PARAMETERS; ++index) {
        parameter_values[index] = MIGA80_INVALID_VALUE;
    }
    for (index = 0; index < MIGA80_MAX_LOCALS; ++index) {
        local_values[index] = MIGA80_INVALID_VALUE;
    }

    for (index = 0; index < source->instruction_count; ++index) {
        const struct miga80_ir_instruction *instruction =
            &source->instructions[index];
        unsigned int value = MIGA80_INVALID_VALUE;

        if (returned) {
            return fail(diagnostic, instruction->line, instruction->column,
                        "typed IR continues after return");
        }
        switch (instruction->opcode) {
        case MIGA80_IR_PUSH_I32:
            value = make_constant(result, instruction->operand,
                                  instruction->line, instruction->column,
                                  diagnostic);
            break;
        case MIGA80_IR_PUSH_PARAMETER_I32:
            if (instruction->operand >= source->parameter_count ||
                instruction->operand >= MIGA80_MAX_PARAMETERS) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR parameter exceeds value ABI");
            }
            value = parameter_values[instruction->operand];
            if (value == MIGA80_INVALID_VALUE) {
                value = add_value(result, MIGA80_VALUE_PARAMETER,
                                  MIGA80_INVALID_VALUE, MIGA80_INVALID_VALUE,
                                  0U, instruction->operand, instruction->line,
                                  instruction->column, diagnostic);
                parameter_values[instruction->operand] = value;
            }
            break;
        case MIGA80_IR_PUSH_LOCAL_I32:
            if (instruction->operand >= source->local_count ||
                local_values[instruction->operand] == MIGA80_INVALID_VALUE) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR local is invalid during value lowering");
            }
            value = local_values[instruction->operand];
            break;
        case MIGA80_IR_STORE_LOCAL_I32:
            if (instruction->operand >= source->local_count ||
                stack_size != 1U) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR local store is invalid during value lowering");
            }
            local_values[instruction->operand] = stack[--stack_size];
            continue;
        case MIGA80_IR_NEG_I32:
            if (stack_size < 1U) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR stack underflow during value lowering");
            }
            value = make_neg(result, stack[--stack_size], instruction->line,
                             instruction->column, diagnostic);
            break;
        case MIGA80_IR_ADD_I32:
        case MIGA80_IR_SUB_I32:
        case MIGA80_IR_MUL_I32:
            if (stack_size < 2U) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR stack underflow during value lowering");
            } else {
                const unsigned int right = stack[--stack_size];
                const unsigned int left = stack[--stack_size];
                enum miga80_value_opcode opcode = MIGA80_VALUE_ADD;

                if (instruction->opcode == MIGA80_IR_SUB_I32) {
                    opcode = MIGA80_VALUE_SUB;
                } else if (instruction->opcode == MIGA80_IR_MUL_I32) {
                    opcode = MIGA80_VALUE_MUL;
                }
                value = make_binary(result, opcode, left, right,
                                    instruction->line, instruction->column,
                                    diagnostic);
            }
            break;
        case MIGA80_IR_RETURN_I32:
            if (stack_size != 1U || index + 1U != source->instruction_count) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR return has invalid value stack");
            }
            result->result = stack[0];
            stack_size = 0U;
            returned = 1;
            continue;
        default:
            return fail(diagnostic, instruction->line, instruction->column,
                        "unknown typed IR instruction during value lowering");
        }

        if (value == MIGA80_INVALID_VALUE) {
            return 0;
        }
        if (stack_size == MIGA80_MAX_IR_STACK) {
            return fail(diagnostic, instruction->line, instruction->column,
                        "value lowering stack overflow");
        }
        stack[stack_size++] = value;
    }

    if (!returned) {
        return fail(diagnostic, 0U, 0U, "typed IR has no value return");
    }
    return mark_live_values(result, diagnostic);
}
