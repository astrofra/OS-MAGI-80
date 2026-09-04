#include "compiler/ir/ir.h"

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

static int emit_instruction(struct miga80_ir_function *ir,
                            enum miga80_ir_opcode opcode, uint32_t operand,
                            unsigned int line, unsigned int column,
                            struct miga80_diagnostic *diagnostic)
{
    struct miga80_ir_instruction *instruction;

    if (ir->instruction_count == MIGA80_MAX_IR_INSTRUCTIONS) {
        return fail(diagnostic, line, column,
                    "typed IR instruction limit exceeded");
    }
    instruction = &ir->instructions[ir->instruction_count++];
    instruction->opcode = opcode;
    instruction->operand = operand;
    instruction->line = line;
    instruction->column = column;
    return 1;
}

static int lower_node(const struct miga80_ast_function *ast, int node_index,
                      struct miga80_ir_function *ir,
                      struct miga80_diagnostic *diagnostic)
{
    const struct miga80_ast_node *node;
    enum miga80_ir_opcode opcode;

    if (node_index < 0 || (unsigned int)node_index >= ast->node_count) {
        return fail(diagnostic, 0U, 0U, "invalid AST node reference");
    }
    node = &ast->nodes[node_index];
    switch (node->kind) {
    case MIGA80_AST_LITERAL_I32:
        return emit_instruction(ir, MIGA80_IR_PUSH_I32, node->value,
                                node->line, node->column, diagnostic);
    case MIGA80_AST_PARAMETER_I32:
        return emit_instruction(ir, MIGA80_IR_PUSH_PARAMETER_I32,
                                node->parameter_index, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_NEG_I32:
        if (!lower_node(ast, node->left, ir, diagnostic)) {
            return 0;
        }
        return emit_instruction(ir, MIGA80_IR_NEG_I32, 0U, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_ADD_I32:
        opcode = MIGA80_IR_ADD_I32;
        break;
    case MIGA80_AST_SUB_I32:
        opcode = MIGA80_IR_SUB_I32;
        break;
    case MIGA80_AST_MUL_I32:
        opcode = MIGA80_IR_MUL_I32;
        break;
    default:
        return fail(diagnostic, node->line, node->column,
                    "unknown AST node kind");
    }

    if (!lower_node(ast, node->left, ir, diagnostic) ||
        !lower_node(ast, node->right, ir, diagnostic)) {
        return 0;
    }
    return emit_instruction(ir, opcode, 0U, node->line, node->column,
                            diagnostic);
}

int miga80_lower_function(const struct miga80_ast_function *ast,
                          struct miga80_ir_function *ir,
                          struct miga80_diagnostic *diagnostic)
{
    if (ast == NULL || ir == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(ir, 0, sizeof(*ir));
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    (void)memcpy(ir->name, ast->name, sizeof(ir->name));
    ir->parameter_count = ast->parameter_count;
    if (!lower_node(ast, ast->result, ir, diagnostic)) {
        return 0;
    }
    return emit_instruction(ir, MIGA80_IR_RETURN_I32, 0U, 0U, 0U,
                            diagnostic);
}

int miga80_evaluate_ir(const struct miga80_ir_function *ir,
                       const uint32_t *arguments, unsigned int argument_count,
                       uint32_t *result,
                       struct miga80_diagnostic *diagnostic)
{
    uint32_t stack[MIGA80_MAX_IR_STACK];
    unsigned int stack_size = 0U;
    unsigned int index;

    if (ir == NULL || result == NULL || diagnostic == NULL ||
        (arguments == NULL && argument_count != 0U)) {
        return 0;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    if (argument_count != ir->parameter_count) {
        return fail(diagnostic, 0U, 0U, "argument count does not match ABI");
    }

    for (index = 0; index < ir->instruction_count; ++index) {
        const struct miga80_ir_instruction *instruction =
            &ir->instructions[index];
        uint32_t left;
        uint32_t right;

        switch (instruction->opcode) {
        case MIGA80_IR_PUSH_I32:
            if (stack_size == MIGA80_MAX_IR_STACK) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack overflow");
            }
            stack[stack_size++] = instruction->operand;
            break;
        case MIGA80_IR_PUSH_PARAMETER_I32:
            if (instruction->operand >= argument_count) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR parameter index is invalid");
            }
            if (stack_size == MIGA80_MAX_IR_STACK) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack overflow");
            }
            stack[stack_size++] = arguments[instruction->operand];
            break;
        case MIGA80_IR_NEG_I32:
            if (stack_size < 1U) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack underflow");
            }
            stack[stack_size - 1U] = 0U - stack[stack_size - 1U];
            break;
        case MIGA80_IR_ADD_I32:
        case MIGA80_IR_SUB_I32:
        case MIGA80_IR_MUL_I32:
            if (stack_size < 2U) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack underflow");
            }
            right = stack[--stack_size];
            left = stack[stack_size - 1U];
            if (instruction->opcode == MIGA80_IR_ADD_I32) {
                stack[stack_size - 1U] = left + right;
            } else if (instruction->opcode == MIGA80_IR_SUB_I32) {
                stack[stack_size - 1U] = left - right;
            } else {
                stack[stack_size - 1U] = left * right;
            }
            break;
        case MIGA80_IR_RETURN_I32:
            if (stack_size != 1U || index + 1U != ir->instruction_count) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR return has invalid stack state");
            }
            *result = stack[0];
            return 1;
        default:
            return fail(diagnostic, instruction->line, instruction->column,
                        "unknown typed IR instruction");
        }
    }
    return fail(diagnostic, 0U, 0U, "typed IR function has no return");
}
