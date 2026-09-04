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
                                node->symbol_index, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_LOCAL_I32:
        return emit_instruction(ir, MIGA80_IR_PUSH_LOCAL_I32,
                                node->symbol_index, node->line,
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
    unsigned int statement_index;
    int returned = 0;

    if (ast == NULL || ir == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(ir, 0, sizeof(*ir));
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    if (ast->parameter_count > MIGA80_MAX_PARAMETERS ||
        ast->local_count > MIGA80_MAX_LOCALS ||
        ast->node_count > MIGA80_MAX_AST_NODES ||
        ast->statement_count > MIGA80_MAX_STATEMENTS) {
        return fail(diagnostic, 0U, 0U,
                    "AST function exceeds bounded storage");
    }
    (void)memcpy(ir->name, ast->name, sizeof(ir->name));
    ir->parameter_count = ast->parameter_count;
    ir->local_count = ast->local_count;
    ir->block_count = 1U;
    ir->entry_block = 0U;
    ir->blocks[0].first_instruction = 0U;
    ir->blocks[0].successors[0] = MIGA80_INVALID_BLOCK;
    ir->blocks[0].successors[1] = MIGA80_INVALID_BLOCK;

    for (statement_index = 0U; statement_index < ast->statement_count;
         ++statement_index) {
        const struct miga80_ast_statement *statement =
            &ast->statements[statement_index];

        if (returned || !lower_node(ast, statement->expression, ir,
                                    diagnostic)) {
            if (returned) {
                return fail(diagnostic, statement->line, statement->column,
                            "AST statement follows return");
            }
            return 0;
        }
        if (statement->kind == MIGA80_AST_LOCAL_INITIALIZE_I32 ||
            statement->kind == MIGA80_AST_LOCAL_ASSIGN_I32) {
            if (statement->local_index >= ast->local_count ||
                !emit_instruction(ir, MIGA80_IR_STORE_LOCAL_I32,
                                  statement->local_index, statement->line,
                                  statement->column, diagnostic)) {
                if (statement->local_index >= ast->local_count) {
                    return fail(diagnostic, statement->line,
                                statement->column,
                                "AST local statement index is invalid");
                }
                return 0;
            }
        } else if (statement->kind == MIGA80_AST_RETURN_I32) {
            if (statement_index + 1U != ast->statement_count ||
                !emit_instruction(ir, MIGA80_IR_RETURN_I32, 0U,
                                  statement->line, statement->column,
                                  diagnostic)) {
                if (statement_index + 1U != ast->statement_count) {
                    return fail(diagnostic, statement->line,
                                statement->column,
                                "AST return is not the final statement");
                }
                return 0;
            }
            returned = 1;
        } else {
            return fail(diagnostic, statement->line, statement->column,
                        "unknown AST statement kind");
        }
    }
    if (!returned) {
        return fail(diagnostic, 0U, 0U, "AST function has no return");
    }
    ir->blocks[0].instruction_count = ir->instruction_count;
    return 1;
}

int miga80_validate_straight_line_ir(
    const struct miga80_ir_function *ir,
    struct miga80_diagnostic *diagnostic)
{
    const struct miga80_ir_basic_block *block;

    if (ir == NULL || diagnostic == NULL) {
        return 0;
    }
    if (ir->parameter_count > MIGA80_MAX_PARAMETERS ||
        ir->local_count > MIGA80_MAX_LOCALS ||
        ir->instruction_count > MIGA80_MAX_IR_INSTRUCTIONS) {
        return fail(diagnostic, 0U, 0U,
                    "typed IR function exceeds bounded storage");
    }
    if (ir->block_count != 1U || ir->entry_block != 0U) {
        return fail(diagnostic, 0U, 0U,
                    "typed IR control flow is not implemented");
    }
    block = &ir->blocks[0];
    if (block->first_instruction != 0U ||
        block->instruction_count != ir->instruction_count ||
        block->successor_count != 0U ||
        block->successors[0] != MIGA80_INVALID_BLOCK ||
        block->successors[1] != MIGA80_INVALID_BLOCK) {
        return fail(diagnostic, 0U, 0U,
                    "typed IR entry block is invalid");
    }
    return 1;
}

int miga80_evaluate_ir(const struct miga80_ir_function *ir,
                       const uint32_t *arguments, unsigned int argument_count,
                       uint32_t *result,
                       struct miga80_diagnostic *diagnostic)
{
    uint32_t stack[MIGA80_MAX_IR_STACK];
    uint32_t locals[MIGA80_MAX_LOCALS];
    unsigned char local_initialized[MIGA80_MAX_LOCALS];
    unsigned int stack_size = 0U;
    unsigned int index;

    if (ir == NULL || result == NULL || diagnostic == NULL ||
        (arguments == NULL && argument_count != 0U)) {
        return 0;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    (void)memset(locals, 0, sizeof(locals));
    (void)memset(local_initialized, 0, sizeof(local_initialized));
    if (!miga80_validate_straight_line_ir(ir, diagnostic)) {
        return 0;
    }
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
        case MIGA80_IR_PUSH_LOCAL_I32:
            if (instruction->operand >= ir->local_count ||
                !local_initialized[instruction->operand]) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR local is invalid or uninitialized");
            }
            if (stack_size == MIGA80_MAX_IR_STACK) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack overflow");
            }
            stack[stack_size++] = locals[instruction->operand];
            break;
        case MIGA80_IR_STORE_LOCAL_I32:
            if (instruction->operand >= ir->local_count ||
                stack_size != 1U) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR local store has invalid stack state");
            }
            locals[instruction->operand] = stack[--stack_size];
            local_initialized[instruction->operand] = 1U;
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
