#ifndef MIGA80_COMPILER_FRONTEND_H
#define MIGA80_COMPILER_FRONTEND_H

#include <stddef.h>
#include <stdint.h>

#include "compiler/abi/abi.h"

#define MIGA80_MAX_NAME 31U
#define MIGA80_MAX_PARAMETERS MIGA80_ABI_MAX_SCALAR_ARGUMENTS
#define MIGA80_MAX_LOCALS 16U
#define MIGA80_MAX_STATEMENTS 32U
#define MIGA80_MAX_AST_NODES 128U
#define MIGA80_INVALID_NODE (-1)

struct miga80_diagnostic {
    unsigned int line;
    unsigned int column;
    char message[160];
};

enum miga80_ast_kind {
    MIGA80_AST_LITERAL_I32,
    MIGA80_AST_PARAMETER_I32,
    MIGA80_AST_LOCAL_I32,
    MIGA80_AST_NEG_I32,
    MIGA80_AST_ADD_I32,
    MIGA80_AST_SUB_I32,
    MIGA80_AST_MUL_I32
};

struct miga80_ast_node {
    enum miga80_ast_kind kind;
    unsigned int line;
    unsigned int column;
    int left;
    int right;
    uint32_t value;
    unsigned int symbol_index;
};

enum miga80_ast_statement_kind {
    MIGA80_AST_LOCAL_INITIALIZE_I32,
    MIGA80_AST_LOCAL_ASSIGN_I32,
    MIGA80_AST_RETURN_I32
};

struct miga80_ast_statement {
    enum miga80_ast_statement_kind kind;
    unsigned int local_index;
    int expression;
    unsigned int line;
    unsigned int column;
};

struct miga80_ast_function {
    char name[MIGA80_MAX_NAME + 1U];
    char parameter_names[MIGA80_MAX_PARAMETERS][MIGA80_MAX_NAME + 1U];
    unsigned int parameter_count;
    char local_names[MIGA80_MAX_LOCALS][MIGA80_MAX_NAME + 1U];
    unsigned int local_count;
    struct miga80_ast_node nodes[MIGA80_MAX_AST_NODES];
    unsigned int node_count;
    struct miga80_ast_statement statements[MIGA80_MAX_STATEMENTS];
    unsigned int statement_count;
    int result;
};

int miga80_parse_function(const char *source, size_t source_size,
                          struct miga80_ast_function *function,
                          struct miga80_diagnostic *diagnostic);

#endif
