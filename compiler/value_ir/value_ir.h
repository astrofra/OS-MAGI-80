#ifndef MIGA80_COMPILER_VALUE_IR_H
#define MIGA80_COMPILER_VALUE_IR_H

#include <limits.h>
#include <stdint.h>

#include "compiler/ir/ir.h"

#define MIGA80_MAX_VALUE_INSTRUCTIONS MIGA80_MAX_IR_INSTRUCTIONS
#define MIGA80_INVALID_VALUE UINT_MAX

enum miga80_value_type {
    MIGA80_VALUE_I32
};

enum miga80_value_opcode {
    MIGA80_VALUE_CONSTANT,
    MIGA80_VALUE_PARAMETER,
    MIGA80_VALUE_NEG,
    MIGA80_VALUE_ADD,
    MIGA80_VALUE_SUB,
    MIGA80_VALUE_MUL
};

struct miga80_value_instruction {
    enum miga80_value_type type;
    enum miga80_value_opcode opcode;
    unsigned int left;
    unsigned int right;
    uint32_t immediate;
    unsigned int parameter_index;
    unsigned int line;
    unsigned int column;
    unsigned int use_count;
    int live;
};

struct miga80_value_function {
    char name[MIGA80_MAX_NAME + 1U];
    unsigned int parameter_count;
    struct miga80_value_instruction values[MIGA80_MAX_VALUE_INSTRUCTIONS];
    unsigned int value_count;
    unsigned int result;
};

int miga80_build_value_ir(const struct miga80_ir_function *source,
                          struct miga80_value_function *result,
                          struct miga80_diagnostic *diagnostic);

#endif
