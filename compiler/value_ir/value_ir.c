#include "compiler/value_ir/value_ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct value_lower_state {
    unsigned int entry_locals[MIGA80_MAX_BASIC_BLOCKS][MIGA80_MAX_LOCALS];
    unsigned int exit_locals[MIGA80_MAX_BASIC_BLOCKS][MIGA80_MAX_LOCALS];
    unsigned char processed[MIGA80_MAX_BASIC_BLOCKS];
};

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
                              enum miga80_type type,
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
    value->type = type;
    value->opcode = opcode;
    value->left = left;
    value->right = right;
    value->immediate = immediate;
    value->parameter_index = parameter_index;
    value->left_block = MIGA80_INVALID_BLOCK;
    value->right_block = MIGA80_INVALID_BLOCK;
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
                                  enum miga80_type type, uint32_t constant,
                                  unsigned int line, unsigned int column,
                                  struct miga80_diagnostic *diagnostic)
{
    return add_value(function, type, MIGA80_VALUE_CONSTANT,
                     MIGA80_INVALID_VALUE, MIGA80_INVALID_VALUE, constant, 0U,
                     line, column, diagnostic);
}

static unsigned int make_neg(struct miga80_value_function *function,
                             unsigned int operand, unsigned int line,
                             unsigned int column,
                             struct miga80_diagnostic *diagnostic)
{
    uint32_t constant;

    if (is_constant(function, operand, &constant)) {
        return make_constant(function, MIGA80_TYPE_I32, 0U - constant, line,
                             column, diagnostic);
    }
    if (function->values[operand].opcode == MIGA80_VALUE_NEG) {
        return function->values[operand].left;
    }
    return add_value(function, MIGA80_TYPE_I32, MIGA80_VALUE_NEG, operand,
                     MIGA80_INVALID_VALUE, 0U, 0U, line, column, diagnostic);
}

static uint32_t fold_comparison(enum miga80_value_opcode opcode,
                                uint32_t left, uint32_t right)
{
    const uint32_t signed_left = left ^ UINT32_C(0x80000000);
    const uint32_t signed_right = right ^ UINT32_C(0x80000000);

    if (opcode == MIGA80_VALUE_EQ) {
        return left == right ? 1U : 0U;
    }
    if (opcode == MIGA80_VALUE_NE) {
        return left != right ? 1U : 0U;
    }
    if (opcode == MIGA80_VALUE_LT_I32) {
        return signed_left < signed_right ? 1U : 0U;
    }
    if (opcode == MIGA80_VALUE_LE_I32) {
        return signed_left <= signed_right ? 1U : 0U;
    }
    if (opcode == MIGA80_VALUE_GT_I32) {
        return signed_left > signed_right ? 1U : 0U;
    }
    return signed_left >= signed_right ? 1U : 0U;
}

static int comparison_opcode(enum miga80_value_opcode opcode)
{
    return opcode >= MIGA80_VALUE_EQ && opcode <= MIGA80_VALUE_GE_I32;
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
        } else if (opcode == MIGA80_VALUE_MUL) {
            folded = left_constant * right_constant;
        } else {
            folded = fold_comparison(opcode, left_constant, right_constant);
        }
        return make_constant(function,
                             comparison_opcode(opcode) ? MIGA80_TYPE_BOOL
                                                       : MIGA80_TYPE_I32,
                             folded, line, column, diagnostic);
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
            return make_constant(function, MIGA80_TYPE_I32, 0U, line, column,
                                 diagnostic);
        }
    }
    if (opcode == MIGA80_VALUE_MUL && right_is_constant) {
        if (right_constant == 0U) {
            return make_constant(function, MIGA80_TYPE_I32, 0U, line, column,
                                 diagnostic);
        }
        if (right_constant == 1U) {
            return left;
        }
    }
    if (comparison_opcode(opcode) && left == right) {
        const uint32_t folded =
            opcode == MIGA80_VALUE_EQ || opcode == MIGA80_VALUE_LE_I32 ||
                    opcode == MIGA80_VALUE_GE_I32
                ? 1U
                : 0U;

        return make_constant(function, MIGA80_TYPE_BOOL, folded, line, column,
                             diagnostic);
    }
    return add_value(function,
                     comparison_opcode(opcode) ? MIGA80_TYPE_BOOL
                                               : MIGA80_TYPE_I32,
                     opcode, left, right, 0U, 0U, line, column, diagnostic);
}

static unsigned int make_phi(struct miga80_value_function *function,
                             enum miga80_type type, unsigned int left,
                             unsigned int left_block, unsigned int right,
                             unsigned int right_block,
                             struct miga80_diagnostic *diagnostic)
{
    unsigned int result;

    if (left == right) {
        return left;
    }
    result = add_value(function, type, MIGA80_VALUE_PHI, left, right, 0U, 0U,
                       0U, 0U, diagnostic);
    if (result != MIGA80_INVALID_VALUE) {
        function->values[result].left_block = left_block;
        function->values[result].right_block = right_block;
    }
    return result;
}

static int opcode_has_left(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_NEG || opcode == MIGA80_VALUE_ADD ||
           opcode == MIGA80_VALUE_SUB || opcode == MIGA80_VALUE_MUL ||
           comparison_opcode(opcode) || opcode == MIGA80_VALUE_PHI;
}

static int opcode_has_right(enum miga80_value_opcode opcode)
{
    return opcode == MIGA80_VALUE_ADD || opcode == MIGA80_VALUE_SUB ||
           opcode == MIGA80_VALUE_MUL || comparison_opcode(opcode) ||
           opcode == MIGA80_VALUE_PHI;
}

static int mark_root(struct miga80_value_function *function,
                     unsigned int value,
                     struct miga80_diagnostic *diagnostic)
{
    if (value >= function->value_count) {
        return fail(diagnostic, 0U, 0U, "value IR root is invalid");
    }
    function->values[value].live = 1;
    ++function->values[value].use_count;
    return 1;
}

static int mark_live_values(struct miga80_value_function *function,
                            struct miga80_diagnostic *diagnostic)
{
    unsigned int block_index;
    unsigned int remaining;

    if (!mark_root(function, function->result, diagnostic)) {
        return 0;
    }
    for (block_index = 0U; block_index < function->block_count;
         ++block_index) {
        if (function->blocks[block_index].terminator == MIGA80_VALUE_BRANCH &&
            !mark_root(function, function->blocks[block_index].condition,
                       diagnostic)) {
            return 0;
        }
    }
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
    return 1;
}

static enum miga80_value_opcode value_opcode(enum miga80_ir_opcode opcode)
{
    switch (opcode) {
    case MIGA80_IR_ADD_I32:
        return MIGA80_VALUE_ADD;
    case MIGA80_IR_SUB_I32:
        return MIGA80_VALUE_SUB;
    case MIGA80_IR_MUL_I32:
        return MIGA80_VALUE_MUL;
    case MIGA80_IR_EQ_I32:
    case MIGA80_IR_EQ_BOOL:
        return MIGA80_VALUE_EQ;
    case MIGA80_IR_NE_I32:
    case MIGA80_IR_NE_BOOL:
        return MIGA80_VALUE_NE;
    case MIGA80_IR_LT_I32:
        return MIGA80_VALUE_LT_I32;
    case MIGA80_IR_LE_I32:
        return MIGA80_VALUE_LE_I32;
    case MIGA80_IR_GT_I32:
        return MIGA80_VALUE_GT_I32;
    default:
        return MIGA80_VALUE_GE_I32;
    }
}

static int build_predecessors(const struct miga80_ir_function *source,
                              struct miga80_value_function *result,
                              struct miga80_diagnostic *diagnostic)
{
    unsigned int block_index;

    for (block_index = 0U; block_index < source->block_count; ++block_index) {
        const struct miga80_ir_basic_block *source_block =
            &source->blocks[block_index];
        struct miga80_value_basic_block *block = &result->blocks[block_index];

        block->condition = MIGA80_INVALID_VALUE;
        block->predecessors[0] = MIGA80_INVALID_BLOCK;
        block->predecessors[1] = MIGA80_INVALID_BLOCK;
        block->successors[0] = source_block->successors[0];
        block->successors[1] = source_block->successors[1];
        block->successor_count = source_block->successor_count;
    }
    for (block_index = 0U; block_index < source->block_count; ++block_index) {
        const struct miga80_ir_basic_block *source_block =
            &source->blocks[block_index];
        unsigned int successor_index;

        for (successor_index = 0U;
             successor_index < source_block->successor_count;
             ++successor_index) {
            struct miga80_value_basic_block *successor =
                &result->blocks[source_block->successors[successor_index]];

            if (successor->predecessor_count ==
                MIGA80_MAX_BLOCK_SUCCESSORS) {
                return fail(diagnostic, 0U, 0U,
                            "value IR join has too many predecessors");
            }
            successor->predecessors[successor->predecessor_count++] =
                block_index;
        }
    }
    return 1;
}

static int predecessors_processed(
    const struct miga80_value_function *function,
    const struct value_lower_state *state, unsigned int block_index)
{
    const struct miga80_value_basic_block *block =
        &function->blocks[block_index];
    unsigned int index;

    for (index = 0U; index < block->predecessor_count; ++index) {
        if (!state->processed[block->predecessors[index]]) {
            return 0;
        }
    }
    return 1;
}

static int merge_local_values(const struct miga80_ir_function *source,
                              struct miga80_value_function *result,
                              struct value_lower_state *state,
                              unsigned int block_index,
                              struct miga80_diagnostic *diagnostic)
{
    const struct miga80_value_basic_block *block =
        &result->blocks[block_index];
    unsigned int local;

    if (block->predecessor_count == 0U) {
        return block_index == result->entry_block;
    }
    if (block->predecessor_count == 1U) {
        (void)memcpy(state->entry_locals[block_index],
                     state->exit_locals[block->predecessors[0]],
                     sizeof(state->entry_locals[block_index]));
        return 1;
    }
    for (local = 0U; local < source->local_count; ++local) {
        const unsigned int left =
            state->exit_locals[block->predecessors[0]][local];
        const unsigned int right =
            state->exit_locals[block->predecessors[1]][local];

        if (left == MIGA80_INVALID_VALUE && right == MIGA80_INVALID_VALUE) {
            state->entry_locals[block_index][local] = MIGA80_INVALID_VALUE;
            continue;
        }
        if (left == MIGA80_INVALID_VALUE || right == MIGA80_INVALID_VALUE) {
            return fail(diagnostic, 0U, 0U,
                        "local is not initialized on every incoming edge");
        }
        state->entry_locals[block_index][local] =
            make_phi(result, source->local_types[local], left,
                     block->predecessors[0], right, block->predecessors[1],
                     diagnostic);
        if (state->entry_locals[block_index][local] == MIGA80_INVALID_VALUE) {
            return 0;
        }
    }
    return 1;
}

static int lower_block_values(const struct miga80_ir_function *source,
                              struct miga80_value_function *result,
                              struct value_lower_state *state,
                              unsigned int block_index,
                              struct miga80_diagnostic *diagnostic)
{
    const struct miga80_ir_basic_block *source_block =
        &source->blocks[block_index];
    struct miga80_value_basic_block *block = &result->blocks[block_index];
    unsigned int stack[MIGA80_MAX_IR_STACK];
    unsigned int stack_size = 0U;
    unsigned int offset;

    block->first_value = result->value_count;
    if (!merge_local_values(source, result, state, block_index, diagnostic)) {
        return 0;
    }
    (void)memcpy(state->exit_locals[block_index],
                 state->entry_locals[block_index],
                 sizeof(state->exit_locals[block_index]));
    for (offset = 0U; offset < source_block->instruction_count; ++offset) {
        const struct miga80_ir_instruction *instruction =
            &source->instructions[source_block->first_instruction + offset];
        unsigned int value = MIGA80_INVALID_VALUE;

        switch (instruction->opcode) {
        case MIGA80_IR_PUSH_I32:
            value = make_constant(result, MIGA80_TYPE_I32,
                                  instruction->operand, instruction->line,
                                  instruction->column, diagnostic);
            break;
        case MIGA80_IR_PUSH_BOOL:
            value = make_constant(result, MIGA80_TYPE_BOOL,
                                  instruction->operand, instruction->line,
                                  instruction->column, diagnostic);
            break;
        case MIGA80_IR_PUSH_PARAMETER_I32:
        case MIGA80_IR_PUSH_PARAMETER_BOOL:
            value = instruction->operand;
            break;
        case MIGA80_IR_PUSH_LOCAL_I32:
        case MIGA80_IR_PUSH_LOCAL_BOOL:
            value = state->exit_locals[block_index][instruction->operand];
            if (value == MIGA80_INVALID_VALUE) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "local is uninitialized during value lowering");
            }
            break;
        case MIGA80_IR_STORE_LOCAL_I32:
        case MIGA80_IR_STORE_LOCAL_BOOL:
            state->exit_locals[block_index][instruction->operand] =
                stack[--stack_size];
            continue;
        case MIGA80_IR_NEG_I32:
            value = make_neg(result, stack[--stack_size], instruction->line,
                             instruction->column, diagnostic);
            break;
        case MIGA80_IR_ADD_I32:
        case MIGA80_IR_SUB_I32:
        case MIGA80_IR_MUL_I32:
        case MIGA80_IR_EQ_I32:
        case MIGA80_IR_NE_I32:
        case MIGA80_IR_EQ_BOOL:
        case MIGA80_IR_NE_BOOL:
        case MIGA80_IR_LT_I32:
        case MIGA80_IR_LE_I32:
        case MIGA80_IR_GT_I32:
        case MIGA80_IR_GE_I32: {
            const unsigned int right = stack[--stack_size];
            const unsigned int left = stack[--stack_size];

            value = make_binary(result, value_opcode(instruction->opcode),
                                left, right, instruction->line,
                                instruction->column, diagnostic);
            break;
        }
        case MIGA80_IR_BRANCH_FALSE:
            block->terminator = MIGA80_VALUE_BRANCH;
            block->condition = stack[--stack_size];
            continue;
        case MIGA80_IR_JUMP:
            block->terminator = MIGA80_VALUE_JUMP;
            continue;
        case MIGA80_IR_RETURN:
            block->terminator = MIGA80_VALUE_RETURN;
            result->result = stack[--stack_size];
            continue;
        default:
            return fail(diagnostic, instruction->line, instruction->column,
                        "unknown typed IR instruction during value lowering");
        }
        if (value == MIGA80_INVALID_VALUE) {
            return 0;
        }
        stack[stack_size++] = value;
    }
    if (stack_size != 0U) {
        return fail(diagnostic, 0U, 0U,
                    "value lowering leaves a non-empty block stack");
    }
    block->value_count = result->value_count - block->first_value;
    state->processed[block_index] = 1U;
    result->block_order[result->block_order_count++] = block_index;
    return 1;
}

int miga80_build_value_ir(const struct miga80_ir_function *source,
                          struct miga80_value_function *result,
                          struct miga80_diagnostic *diagnostic)
{
    struct value_lower_state *state;
    unsigned int block_index;
    unsigned int local_index;
    unsigned int processed_count = 0U;
    unsigned int index;
    int success = 0;

    if (source == NULL || result == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(result, 0, sizeof(*result));
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    if (!miga80_validate_ir(source, diagnostic)) {
        return 0;
    }
    (void)memcpy(result->name, source->name, sizeof(result->name));
    (void)memcpy(result->parameter_types, source->parameter_types,
                 sizeof(result->parameter_types));
    result->parameter_count = source->parameter_count;
    result->result_type = source->result_type;
    result->block_count = source->block_count;
    result->entry_block = source->entry_block;
    result->result = MIGA80_INVALID_VALUE;
    if (!build_predecessors(source, result, diagnostic)) {
        return 0;
    }
    for (index = 0U; index < source->parameter_count; ++index) {
        if (add_value(result, source->parameter_types[index],
                      MIGA80_VALUE_PARAMETER, MIGA80_INVALID_VALUE,
                      MIGA80_INVALID_VALUE, 0U, index, 0U, 0U, diagnostic) ==
            MIGA80_INVALID_VALUE) {
            return 0;
        }
    }
    state = (struct value_lower_state *)malloc(sizeof(*state));
    if (state == NULL) {
        return fail(diagnostic, 0U, 0U,
                    "unable to allocate value CFG state");
    }
    (void)memset(state, 0, sizeof(*state));
    for (block_index = 0U; block_index < MIGA80_MAX_BASIC_BLOCKS;
         ++block_index) {
        for (local_index = 0U; local_index < MIGA80_MAX_LOCALS;
             ++local_index) {
            state->entry_locals[block_index][local_index] =
                MIGA80_INVALID_VALUE;
            state->exit_locals[block_index][local_index] =
                MIGA80_INVALID_VALUE;
        }
    }
    while (processed_count < source->block_count) {
        int progressed = 0;

        for (index = 0U; index < source->block_count; ++index) {
            if (!state->processed[index] &&
                predecessors_processed(result, state, index)) {
                if (!lower_block_values(source, result, state, index,
                                        diagnostic)) {
                    goto done;
                }
                ++processed_count;
                progressed = 1;
            }
        }
        if (!progressed) {
            (void)fail(diagnostic, 0U, 0U,
                       "value IR loop lowering is not implemented");
            goto done;
        }
    }
    if (result->result == MIGA80_INVALID_VALUE) {
        (void)fail(diagnostic, 0U, 0U, "typed IR has no value return");
        goto done;
    }
    success = mark_live_values(result, diagnostic);

done:
    free(state);
    return success;
}
