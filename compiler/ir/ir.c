#include "compiler/ir/ir.h"

#include <stdio.h>
#include <string.h>

#define MIGA80_MAX_EVALUATED_BLOCKS (MIGA80_MAX_BASIC_BLOCKS * 1024U)

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
    case MIGA80_AST_LITERAL_BOOL:
        return emit_instruction(ir, MIGA80_IR_PUSH_BOOL, node->value,
                                node->line, node->column, diagnostic);
    case MIGA80_AST_PARAMETER_I32:
        return emit_instruction(ir, MIGA80_IR_PUSH_PARAMETER_I32,
                                node->symbol_index, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_PARAMETER_BOOL:
        return emit_instruction(ir, MIGA80_IR_PUSH_PARAMETER_BOOL,
                                node->symbol_index, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_LOCAL_I32:
        return emit_instruction(ir, MIGA80_IR_PUSH_LOCAL_I32,
                                node->symbol_index, node->line,
                                node->column, diagnostic);
    case MIGA80_AST_LOCAL_BOOL:
        return emit_instruction(ir, MIGA80_IR_PUSH_LOCAL_BOOL,
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
    case MIGA80_AST_EQ:
        if (node->left < 0 || (unsigned int)node->left >= ast->node_count) {
            return fail(diagnostic, node->line, node->column,
                        "invalid AST comparison operand");
        }
        opcode = ast->nodes[node->left].type == MIGA80_TYPE_BOOL
                     ? MIGA80_IR_EQ_BOOL
                     : MIGA80_IR_EQ_I32;
        break;
    case MIGA80_AST_NE:
        if (node->left < 0 || (unsigned int)node->left >= ast->node_count) {
            return fail(diagnostic, node->line, node->column,
                        "invalid AST comparison operand");
        }
        opcode = ast->nodes[node->left].type == MIGA80_TYPE_BOOL
                     ? MIGA80_IR_NE_BOOL
                     : MIGA80_IR_NE_I32;
        break;
    case MIGA80_AST_LT_I32:
        opcode = MIGA80_IR_LT_I32;
        break;
    case MIGA80_AST_LE_I32:
        opcode = MIGA80_IR_LE_I32;
        break;
    case MIGA80_AST_GT_I32:
        opcode = MIGA80_IR_GT_I32;
        break;
    case MIGA80_AST_GE_I32:
        opcode = MIGA80_IR_GE_I32;
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

struct lower_context {
    const struct miga80_ast_function *ast;
    struct miga80_ir_function *ir;
    struct miga80_diagnostic *diagnostic;
    unsigned int visited_statements;
    int returned;
};

static unsigned int create_block(struct lower_context *context)
{
    struct miga80_ir_basic_block *block;
    unsigned int index;

    if (context->ir->block_count == MIGA80_MAX_BASIC_BLOCKS) {
        (void)fail(context->diagnostic, 0U, 0U,
                   "typed IR basic-block limit exceeded");
        return MIGA80_INVALID_BLOCK;
    }
    index = context->ir->block_count++;
    block = &context->ir->blocks[index];
    (void)memset(block, 0, sizeof(*block));
    block->first_instruction = MIGA80_INVALID_BLOCK;
    block->successors[0] = MIGA80_INVALID_BLOCK;
    block->successors[1] = MIGA80_INVALID_BLOCK;
    return index;
}

static void begin_block(struct lower_context *context, unsigned int block)
{
    context->ir->blocks[block].first_instruction =
        context->ir->instruction_count;
}

static int finish_block(struct lower_context *context, unsigned int block,
                        unsigned int first_successor,
                        unsigned int second_successor,
                        unsigned int successor_count)
{
    struct miga80_ir_basic_block *result = &context->ir->blocks[block];

    if (result->first_instruction == MIGA80_INVALID_BLOCK ||
        successor_count > MIGA80_MAX_BLOCK_SUCCESSORS) {
        return fail(context->diagnostic, 0U, 0U,
                    "unable to finish typed IR basic block");
    }
    result->instruction_count =
        context->ir->instruction_count - result->first_instruction;
    result->successor_count = successor_count;
    if (successor_count >= 1U) {
        result->successors[0] = first_successor;
    }
    if (successor_count >= 2U) {
        result->successors[1] = second_successor;
    }
    return 1;
}

static int lower_statement_list(struct lower_context *context,
                                unsigned int statement_index,
                                unsigned int *current_block)
{
    while (statement_index != MIGA80_INVALID_STATEMENT) {
        const struct miga80_ast_statement *statement;

        if (statement_index >= context->ast->statement_count ||
            context->visited_statements++ == context->ast->statement_count) {
            return fail(context->diagnostic, 0U, 0U,
                        "invalid or cyclic AST statement list");
        }
        statement = &context->ast->statements[statement_index];
        if (context->returned) {
            return fail(context->diagnostic, statement->line,
                        statement->column, "AST statement follows return");
        }
        if (statement->kind == MIGA80_AST_LOCAL_INITIALIZE ||
            statement->kind == MIGA80_AST_LOCAL_ASSIGN) {
            enum miga80_ir_opcode store_opcode;

            if (statement->local_index >= context->ast->local_count ||
                !lower_node(context->ast, statement->expression, context->ir,
                            context->diagnostic)) {
                if (statement->local_index >= context->ast->local_count) {
                    return fail(context->diagnostic, statement->line,
                                statement->column,
                                "AST local statement index is invalid");
                }
                return 0;
            }
            store_opcode =
                context->ast->local_types[statement->local_index] ==
                        MIGA80_TYPE_BOOL
                    ? MIGA80_IR_STORE_LOCAL_BOOL
                    : MIGA80_IR_STORE_LOCAL_I32;
            if (!emit_instruction(context->ir, store_opcode,
                                  statement->local_index, statement->line,
                                  statement->column, context->diagnostic)) {
                return 0;
            }
        } else if (statement->kind == MIGA80_AST_IF) {
            unsigned int then_block;
            unsigned int else_block;
            unsigned int join_block;
            unsigned int then_end;
            unsigned int else_end;

            if (!lower_node(context->ast, statement->expression, context->ir,
                            context->diagnostic)) {
                return 0;
            }
            then_block = create_block(context);
            else_block = create_block(context);
            join_block = create_block(context);
            if (then_block == MIGA80_INVALID_BLOCK ||
                else_block == MIGA80_INVALID_BLOCK ||
                join_block == MIGA80_INVALID_BLOCK ||
                !emit_instruction(context->ir, MIGA80_IR_BRANCH_FALSE,
                                  else_block, statement->line,
                                  statement->column, context->diagnostic) ||
                !finish_block(context, *current_block, then_block, else_block,
                              2U)) {
                return 0;
            }

            begin_block(context, then_block);
            then_end = then_block;
            if (!lower_statement_list(context, statement->then_statement,
                                      &then_end) ||
                !emit_instruction(context->ir, MIGA80_IR_JUMP, join_block,
                                  statement->line, statement->column,
                                  context->diagnostic) ||
                !finish_block(context, then_end, join_block,
                              MIGA80_INVALID_BLOCK, 1U)) {
                return 0;
            }

            begin_block(context, else_block);
            else_end = else_block;
            if (!lower_statement_list(context, statement->else_statement,
                                      &else_end) ||
                !emit_instruction(context->ir, MIGA80_IR_JUMP, join_block,
                                  statement->line, statement->column,
                                  context->diagnostic) ||
                !finish_block(context, else_end, join_block,
                              MIGA80_INVALID_BLOCK, 1U)) {
                return 0;
            }
            begin_block(context, join_block);
            *current_block = join_block;
        } else if (statement->kind == MIGA80_AST_WHILE) {
            unsigned int header_block;
            unsigned int body_block;
            unsigned int latch_block;
            unsigned int exit_block;
            unsigned int body_end;

            header_block = create_block(context);
            body_block = create_block(context);
            latch_block = create_block(context);
            exit_block = create_block(context);
            if (header_block == MIGA80_INVALID_BLOCK ||
                body_block == MIGA80_INVALID_BLOCK ||
                latch_block == MIGA80_INVALID_BLOCK ||
                exit_block == MIGA80_INVALID_BLOCK ||
                !emit_instruction(context->ir, MIGA80_IR_JUMP,
                                  header_block, statement->line,
                                  statement->column, context->diagnostic) ||
                !finish_block(context, *current_block, header_block,
                              MIGA80_INVALID_BLOCK, 1U)) {
                return 0;
            }

            begin_block(context, header_block);
            if (!lower_node(context->ast, statement->expression,
                            context->ir, context->diagnostic) ||
                !emit_instruction(context->ir, MIGA80_IR_BRANCH_FALSE,
                                  exit_block, statement->line,
                                  statement->column, context->diagnostic) ||
                !finish_block(context, header_block, body_block, exit_block,
                              2U)) {
                return 0;
            }

            begin_block(context, body_block);
            body_end = body_block;
            if (!lower_statement_list(context, statement->then_statement,
                                      &body_end) ||
                !emit_instruction(context->ir, MIGA80_IR_JUMP,
                                  latch_block, statement->line,
                                  statement->column, context->diagnostic) ||
                !finish_block(context, body_end, latch_block,
                              MIGA80_INVALID_BLOCK, 1U)) {
                return 0;
            }

            begin_block(context, latch_block);
            if (!emit_instruction(context->ir, MIGA80_IR_JUMP,
                                  header_block, statement->line,
                                  statement->column, context->diagnostic) ||
                !finish_block(context, latch_block, header_block,
                              MIGA80_INVALID_BLOCK, 1U)) {
                return 0;
            }
            begin_block(context, exit_block);
            *current_block = exit_block;
        } else if (statement->kind == MIGA80_AST_RETURN) {
            if (statement->next_statement != MIGA80_INVALID_STATEMENT ||
                !lower_node(context->ast, statement->expression, context->ir,
                            context->diagnostic) ||
                !emit_instruction(context->ir, MIGA80_IR_RETURN, 0U,
                                  statement->line, statement->column,
                                  context->diagnostic) ||
                !finish_block(context, *current_block,
                              MIGA80_INVALID_BLOCK, MIGA80_INVALID_BLOCK,
                              0U)) {
                return 0;
            }
            context->returned = 1;
        } else {
            return fail(context->diagnostic, statement->line,
                        statement->column, "unknown AST statement kind");
        }
        statement_index = statement->next_statement;
    }
    return 1;
}

int miga80_lower_function(const struct miga80_ast_function *ast,
                          struct miga80_ir_function *ir,
                          struct miga80_diagnostic *diagnostic)
{
    struct lower_context context;
    unsigned int current_block;

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
    (void)memcpy(ir->parameter_types, ast->parameter_types,
                 sizeof(ir->parameter_types));
    (void)memcpy(ir->local_types, ast->local_types,
                 sizeof(ir->local_types));
    ir->parameter_count = ast->parameter_count;
    ir->local_count = ast->local_count;
    ir->result_type = ast->result_type;
    (void)memset(&context, 0, sizeof(context));
    context.ast = ast;
    context.ir = ir;
    context.diagnostic = diagnostic;
    current_block = create_block(&context);
    if (current_block == MIGA80_INVALID_BLOCK) {
        return 0;
    }
    ir->entry_block = current_block;
    begin_block(&context, current_block);
    if (!lower_statement_list(&context, ast->first_statement,
                              &current_block) ||
        !context.returned ||
        context.visited_statements != ast->statement_count) {
        if (diagnostic->message[0] == '\0') {
            return fail(diagnostic, 0U, 0U,
                        "AST function has invalid statement coverage");
        }
        return 0;
    }
    return miga80_validate_ir(ir, diagnostic);
}

static int validate_block_stack(const struct miga80_ir_function *ir,
                                const struct miga80_ir_basic_block *block,
                                struct miga80_diagnostic *diagnostic)
{
    enum miga80_type stack[MIGA80_MAX_IR_STACK];
    unsigned int stack_size = 0U;
    unsigned int offset;

    if (block->instruction_count == 0U) {
        return fail(diagnostic, 0U, 0U, "typed IR basic block is empty");
    }

    for (offset = 0U; offset < block->instruction_count; ++offset) {
        const struct miga80_ir_instruction *instruction =
            &ir->instructions[block->first_instruction + offset];
        const int terminal =
            instruction->opcode == MIGA80_IR_BRANCH_FALSE ||
            instruction->opcode == MIGA80_IR_JUMP ||
            instruction->opcode == MIGA80_IR_RETURN;

        if (terminal && offset + 1U != block->instruction_count) {
            return fail(diagnostic, instruction->line, instruction->column,
                        "typed IR terminator is not last in block");
        }
        if (instruction->opcode == MIGA80_IR_PUSH_I32 ||
            instruction->opcode == MIGA80_IR_PUSH_BOOL ||
            instruction->opcode == MIGA80_IR_PUSH_PARAMETER_I32 ||
            instruction->opcode == MIGA80_IR_PUSH_PARAMETER_BOOL ||
            instruction->opcode == MIGA80_IR_PUSH_LOCAL_I32 ||
            instruction->opcode == MIGA80_IR_PUSH_LOCAL_BOOL) {
            enum miga80_type type =
                instruction->opcode == MIGA80_IR_PUSH_BOOL ||
                        instruction->opcode == MIGA80_IR_PUSH_PARAMETER_BOOL ||
                        instruction->opcode == MIGA80_IR_PUSH_LOCAL_BOOL
                    ? MIGA80_TYPE_BOOL
                    : MIGA80_TYPE_I32;

            if (stack_size == MIGA80_MAX_IR_STACK) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR stack overflow");
            }
            if ((instruction->opcode == MIGA80_IR_PUSH_BOOL &&
                 instruction->operand > 1U) ||
                ((instruction->opcode == MIGA80_IR_PUSH_PARAMETER_I32 ||
                  instruction->opcode == MIGA80_IR_PUSH_PARAMETER_BOOL) &&
                 (instruction->operand >= ir->parameter_count ||
                  ir->parameter_types[instruction->operand] != type)) ||
                ((instruction->opcode == MIGA80_IR_PUSH_LOCAL_I32 ||
                  instruction->opcode == MIGA80_IR_PUSH_LOCAL_BOOL) &&
                 (instruction->operand >= ir->local_count ||
                  ir->local_types[instruction->operand] != type))) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR scalar load has invalid type or index");
            }
            stack[stack_size++] = type;
        } else if (instruction->opcode == MIGA80_IR_STORE_LOCAL_I32 ||
                   instruction->opcode == MIGA80_IR_STORE_LOCAL_BOOL) {
            const enum miga80_type type =
                instruction->opcode == MIGA80_IR_STORE_LOCAL_BOOL
                    ? MIGA80_TYPE_BOOL
                    : MIGA80_TYPE_I32;

            if (stack_size != 1U || instruction->operand >= ir->local_count ||
                ir->local_types[instruction->operand] != type ||
                stack[0] != type) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR local store has invalid type or stack");
            }
            stack_size = 0U;
        } else if (instruction->opcode == MIGA80_IR_NEG_I32) {
            if (stack_size < 1U || stack[stack_size - 1U] != MIGA80_TYPE_I32) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR unary operation has invalid type");
            }
        } else if (instruction->opcode == MIGA80_IR_ADD_I32 ||
                   instruction->opcode == MIGA80_IR_SUB_I32 ||
                   instruction->opcode == MIGA80_IR_MUL_I32 ||
                   instruction->opcode == MIGA80_IR_EQ_I32 ||
                   instruction->opcode == MIGA80_IR_NE_I32 ||
                   instruction->opcode == MIGA80_IR_EQ_BOOL ||
                   instruction->opcode == MIGA80_IR_NE_BOOL ||
                   instruction->opcode == MIGA80_IR_LT_I32 ||
                   instruction->opcode == MIGA80_IR_LE_I32 ||
                   instruction->opcode == MIGA80_IR_GT_I32 ||
                   instruction->opcode == MIGA80_IR_GE_I32) {
            const int comparison =
                instruction->opcode >= MIGA80_IR_EQ_I32 &&
                instruction->opcode <= MIGA80_IR_GE_I32;
            const enum miga80_type operand_type =
                instruction->opcode == MIGA80_IR_EQ_BOOL ||
                        instruction->opcode == MIGA80_IR_NE_BOOL
                    ? MIGA80_TYPE_BOOL
                    : MIGA80_TYPE_I32;

            if (stack_size < 2U || stack[stack_size - 1U] != operand_type ||
                stack[stack_size - 2U] != operand_type) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR binary operation has invalid type");
            }
            --stack_size;
            stack[stack_size - 1U] =
                comparison ? MIGA80_TYPE_BOOL : MIGA80_TYPE_I32;
        } else if (instruction->opcode == MIGA80_IR_BRANCH_FALSE) {
            if (stack_size != 1U || stack[0] != MIGA80_TYPE_BOOL ||
                block->successor_count != 2U ||
                instruction->operand != block->successors[1]) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR conditional branch is invalid");
            }
            stack_size = 0U;
        } else if (instruction->opcode == MIGA80_IR_JUMP) {
            if (stack_size != 0U || block->successor_count != 1U ||
                instruction->operand != block->successors[0]) {
                return fail(diagnostic, instruction->line,
                            instruction->column, "typed IR jump is invalid");
            }
        } else if (instruction->opcode == MIGA80_IR_RETURN) {
            if (stack_size != 1U || stack[0] != ir->result_type ||
                block->successor_count != 0U) {
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "typed IR return has invalid type or stack");
            }
            stack_size = 0U;
        } else {
            return fail(diagnostic, instruction->line, instruction->column,
                        "unknown typed IR instruction");
        }
    }
    {
        const enum miga80_ir_opcode terminator =
            ir->instructions[block->first_instruction +
                             block->instruction_count - 1U]
                .opcode;

        if (terminator != MIGA80_IR_BRANCH_FALSE &&
            terminator != MIGA80_IR_JUMP &&
            terminator != MIGA80_IR_RETURN) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR basic block has no terminator");
        }
    }
    return 1;
}

int miga80_validate_ir(const struct miga80_ir_function *ir,
                       struct miga80_diagnostic *diagnostic)
{
    unsigned char instruction_owners[MIGA80_MAX_IR_INSTRUCTIONS];
    unsigned int block_index;
    unsigned int index;

    if (ir == NULL || diagnostic == NULL) {
        return 0;
    }
    if (ir->parameter_count > MIGA80_MAX_PARAMETERS ||
        ir->local_count > MIGA80_MAX_LOCALS ||
        ir->instruction_count > MIGA80_MAX_IR_INSTRUCTIONS ||
        ir->block_count == 0U ||
        ir->block_count > MIGA80_MAX_BASIC_BLOCKS ||
        ir->entry_block >= ir->block_count) {
        return fail(diagnostic, 0U, 0U,
                    "typed IR function exceeds bounded storage");
    }
    (void)memset(instruction_owners, 0, sizeof(instruction_owners));
    for (block_index = 0U; block_index < ir->block_count; ++block_index) {
        const struct miga80_ir_basic_block *block = &ir->blocks[block_index];

        if (block->instruction_count == 0U ||
            block->first_instruction >= ir->instruction_count ||
            block->instruction_count >
                ir->instruction_count - block->first_instruction ||
            block->successor_count > MIGA80_MAX_BLOCK_SUCCESSORS) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR basic block has invalid bounds");
        }
        for (index = 0U; index < MIGA80_MAX_BLOCK_SUCCESSORS; ++index) {
            if ((index < block->successor_count &&
                 block->successors[index] >= ir->block_count) ||
                (index >= block->successor_count &&
                 block->successors[index] != MIGA80_INVALID_BLOCK)) {
                return fail(diagnostic, 0U, 0U,
                            "typed IR basic block has invalid successors");
            }
        }
        for (index = block->first_instruction;
             index < block->first_instruction + block->instruction_count;
             ++index) {
            if (instruction_owners[index] != 0U) {
                return fail(diagnostic, 0U, 0U,
                            "typed IR basic blocks overlap");
            }
            instruction_owners[index] = 1U;
        }
        if (!validate_block_stack(ir, block, diagnostic)) {
            return 0;
        }
        if (ir->instructions[block->first_instruction +
                             block->instruction_count - 1U]
                    .opcode == MIGA80_IR_BRANCH_FALSE &&
            ir->blocks[block->successors[0]].first_instruction !=
                block->first_instruction + block->instruction_count) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR true branch is not the fallthrough block");
        }
    }
    for (index = 0U; index < ir->instruction_count; ++index) {
        if (instruction_owners[index] == 0U) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR instruction is outside all blocks");
        }
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
    unsigned int current_block;
    unsigned int executed_blocks = 0U;
    unsigned int index;

    if (ir == NULL || result == NULL || diagnostic == NULL ||
        (arguments == NULL && argument_count != 0U)) {
        return 0;
    }
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    (void)memset(locals, 0, sizeof(locals));
    (void)memset(local_initialized, 0, sizeof(local_initialized));
    if (!miga80_validate_ir(ir, diagnostic)) {
        return 0;
    }
    if (argument_count != ir->parameter_count) {
        return fail(diagnostic, 0U, 0U, "argument count does not match ABI");
    }

    for (index = 0U; index < argument_count; ++index) {
        if (ir->parameter_types[index] == MIGA80_TYPE_BOOL &&
            arguments[index] > 1U) {
            return fail(diagnostic, 0U, 0U,
                        "bool argument is not canonical");
        }
    }
    current_block = ir->entry_block;
    for (;;) {
        const struct miga80_ir_basic_block *block =
            &ir->blocks[current_block];
        int transferred = 0;
        unsigned int offset;

        if (++executed_blocks > MIGA80_MAX_EVALUATED_BLOCKS) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR control-flow budget exceeded");
        }
        for (offset = 0U; offset < block->instruction_count; ++offset) {
            const struct miga80_ir_instruction *instruction =
                &ir->instructions[block->first_instruction + offset];
            uint32_t left;
            uint32_t right;

            switch (instruction->opcode) {
            case MIGA80_IR_PUSH_I32:
            case MIGA80_IR_PUSH_BOOL:
                stack[stack_size++] = instruction->operand;
                break;
            case MIGA80_IR_PUSH_PARAMETER_I32:
            case MIGA80_IR_PUSH_PARAMETER_BOOL:
                stack[stack_size++] = arguments[instruction->operand];
                break;
            case MIGA80_IR_PUSH_LOCAL_I32:
            case MIGA80_IR_PUSH_LOCAL_BOOL:
                if (!local_initialized[instruction->operand]) {
                    return fail(diagnostic, instruction->line,
                                instruction->column,
                                "typed IR local is uninitialized");
                }
                stack[stack_size++] = locals[instruction->operand];
                break;
            case MIGA80_IR_STORE_LOCAL_I32:
            case MIGA80_IR_STORE_LOCAL_BOOL:
                locals[instruction->operand] = stack[--stack_size];
                local_initialized[instruction->operand] = 1U;
                break;
            case MIGA80_IR_NEG_I32:
                stack[stack_size - 1U] = 0U - stack[stack_size - 1U];
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
            case MIGA80_IR_GE_I32:
                right = stack[--stack_size];
                left = stack[stack_size - 1U];
                if (instruction->opcode == MIGA80_IR_ADD_I32) {
                    stack[stack_size - 1U] = left + right;
                } else if (instruction->opcode == MIGA80_IR_SUB_I32) {
                    stack[stack_size - 1U] = left - right;
                } else if (instruction->opcode == MIGA80_IR_MUL_I32) {
                    stack[stack_size - 1U] = left * right;
                } else if (instruction->opcode == MIGA80_IR_EQ_I32 ||
                           instruction->opcode == MIGA80_IR_EQ_BOOL) {
                    stack[stack_size - 1U] = left == right ? 1U : 0U;
                } else if (instruction->opcode == MIGA80_IR_NE_I32 ||
                           instruction->opcode == MIGA80_IR_NE_BOOL) {
                    stack[stack_size - 1U] = left != right ? 1U : 0U;
                } else if (instruction->opcode == MIGA80_IR_LT_I32) {
                    stack[stack_size - 1U] =
                        (left ^ UINT32_C(0x80000000)) <
                                (right ^ UINT32_C(0x80000000))
                            ? 1U
                            : 0U;
                } else if (instruction->opcode == MIGA80_IR_LE_I32) {
                    stack[stack_size - 1U] =
                        (left ^ UINT32_C(0x80000000)) <=
                                (right ^ UINT32_C(0x80000000))
                            ? 1U
                            : 0U;
                } else if (instruction->opcode == MIGA80_IR_GT_I32) {
                    stack[stack_size - 1U] =
                        (left ^ UINT32_C(0x80000000)) >
                                (right ^ UINT32_C(0x80000000))
                            ? 1U
                            : 0U;
                } else {
                    stack[stack_size - 1U] =
                        (left ^ UINT32_C(0x80000000)) >=
                                (right ^ UINT32_C(0x80000000))
                            ? 1U
                            : 0U;
                }
                break;
            case MIGA80_IR_BRANCH_FALSE:
                current_block = stack[--stack_size] != 0U
                                    ? block->successors[0]
                                    : block->successors[1];
                transferred = 1;
                break;
            case MIGA80_IR_JUMP:
                current_block = block->successors[0];
                transferred = 1;
                break;
            case MIGA80_IR_RETURN:
                *result = stack[0];
                return 1;
            default:
                return fail(diagnostic, instruction->line,
                            instruction->column,
                            "unknown typed IR instruction");
            }
            if (transferred) {
                break;
            }
        }
        if (!transferred) {
            return fail(diagnostic, 0U, 0U,
                        "typed IR block has no control transfer");
        }
    }
}
