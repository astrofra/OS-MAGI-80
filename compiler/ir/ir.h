#ifndef MIGA80_COMPILER_IR_H
#define MIGA80_COMPILER_IR_H

#include <stdint.h>

#include "compiler/frontend/frontend.h"

#define MIGA80_MAX_IR_INSTRUCTIONS (MIGA80_MAX_AST_NODES + 1U)
#define MIGA80_MAX_IR_STACK MIGA80_MAX_AST_NODES

enum miga80_ir_opcode {
    MIGA80_IR_PUSH_I32,
    MIGA80_IR_PUSH_PARAMETER_I32,
    MIGA80_IR_NEG_I32,
    MIGA80_IR_ADD_I32,
    MIGA80_IR_SUB_I32,
    MIGA80_IR_MUL_I32,
    MIGA80_IR_RETURN_I32
};

struct miga80_ir_instruction {
    enum miga80_ir_opcode opcode;
    uint32_t operand;
    unsigned int line;
    unsigned int column;
};

struct miga80_ir_function {
    char name[MIGA80_MAX_NAME + 1U];
    unsigned int parameter_count;
    struct miga80_ir_instruction instructions[MIGA80_MAX_IR_INSTRUCTIONS];
    unsigned int instruction_count;
};

int miga80_lower_function(const struct miga80_ast_function *ast,
                          struct miga80_ir_function *ir,
                          struct miga80_diagnostic *diagnostic);
int miga80_evaluate_ir(const struct miga80_ir_function *ir,
                       const uint32_t *arguments, unsigned int argument_count,
                       uint32_t *result,
                       struct miga80_diagnostic *diagnostic);

#endif
