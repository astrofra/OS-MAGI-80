#include "compiler/frontend/frontend.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum token_kind {
    TOKEN_EOF,
    TOKEN_INVALID,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_FUNCTION,
    TOKEN_LOCAL,
    TOKEN_IF,
    TOKEN_THEN,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_END,
    TOKEN_I32,
    TOKEN_BOOL,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR
};

struct token {
    enum token_kind kind;
    const char *start;
    size_t length;
    unsigned int line;
    unsigned int column;
    uint32_t integer;
};

struct lexer {
    const char *source;
    size_t size;
    size_t offset;
    unsigned int line;
    unsigned int column;
    struct miga80_diagnostic *diagnostic;
};

struct parser {
    struct lexer lexer;
    struct token current;
    struct miga80_ast_function *function;
    struct miga80_diagnostic *diagnostic;
    int failed;
};

static void set_diagnostic(struct miga80_diagnostic *diagnostic,
                           unsigned int line, unsigned int column,
                           const char *format, ...)
{
    va_list arguments;

    if (diagnostic->message[0] != '\0') {
        return;
    }
    diagnostic->line = line;
    diagnostic->column = column;
    va_start(arguments, format);
    (void)vsnprintf(diagnostic->message, sizeof(diagnostic->message), format,
                    arguments);
    va_end(arguments);
}

static int identifier_start(char character)
{
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') || character == '_';
}

static int identifier_continue(char character)
{
    return identifier_start(character) ||
           (character >= '0' && character <= '9');
}

static char peek(const struct lexer *lexer, size_t distance)
{
    if (lexer->offset + distance >= lexer->size) {
        return '\0';
    }
    return lexer->source[lexer->offset + distance];
}

static char advance(struct lexer *lexer)
{
    const char character = lexer->source[lexer->offset++];

    if (character == '\n') {
        ++lexer->line;
        lexer->column = 1U;
    } else {
        ++lexer->column;
    }
    return character;
}

static void skip_trivia(struct lexer *lexer)
{
    for (;;) {
        const char character = peek(lexer, 0U);

        if (character == ' ' || character == '\t' || character == '\r' ||
            character == '\n') {
            (void)advance(lexer);
        } else if (character == '-' && peek(lexer, 1U) == '-') {
            while (peek(lexer, 0U) != '\0' && peek(lexer, 0U) != '\n') {
                (void)advance(lexer);
            }
        } else {
            return;
        }
    }
}

static int token_is_word(const struct token *token, const char *word)
{
    const size_t length = strlen(word);

    return token->length == length &&
           memcmp(token->start, word, length) == 0;
}

static struct token next_token(struct lexer *lexer)
{
    struct token token;
    char character;

    skip_trivia(lexer);
    (void)memset(&token, 0, sizeof(token));
    token.start = &lexer->source[lexer->offset];
    token.line = lexer->line;
    token.column = lexer->column;
    if (lexer->offset == lexer->size) {
        token.kind = TOKEN_EOF;
        return token;
    }

    character = advance(lexer);
    token.length = 1U;
    if (identifier_start(character)) {
        while (identifier_continue(peek(lexer, 0U))) {
            (void)advance(lexer);
        }
        token.length = (size_t)(&lexer->source[lexer->offset] - token.start);
        token.kind = TOKEN_IDENTIFIER;
        if (token_is_word(&token, "function")) {
            token.kind = TOKEN_FUNCTION;
        } else if (token_is_word(&token, "local")) {
            token.kind = TOKEN_LOCAL;
        } else if (token_is_word(&token, "if")) {
            token.kind = TOKEN_IF;
        } else if (token_is_word(&token, "then")) {
            token.kind = TOKEN_THEN;
        } else if (token_is_word(&token, "else")) {
            token.kind = TOKEN_ELSE;
        } else if (token_is_word(&token, "return")) {
            token.kind = TOKEN_RETURN;
        } else if (token_is_word(&token, "end")) {
            token.kind = TOKEN_END;
        } else if (token_is_word(&token, "i32")) {
            token.kind = TOKEN_I32;
        } else if (token_is_word(&token, "bool")) {
            token.kind = TOKEN_BOOL;
        } else if (token_is_word(&token, "true")) {
            token.kind = TOKEN_TRUE;
        } else if (token_is_word(&token, "false")) {
            token.kind = TOKEN_FALSE;
        }
        return token;
    }

    if (character >= '0' && character <= '9') {
        uint64_t value = (uint64_t)(character - '0');

        while (peek(lexer, 0U) >= '0' && peek(lexer, 0U) <= '9') {
            const uint64_t digit = (uint64_t)(peek(lexer, 0U) - '0');

            if (value > (UINT32_C(2147483647) - digit) / 10U) {
                set_diagnostic(lexer->diagnostic, token.line, token.column,
                               "i32 literal is out of range");
                token.kind = TOKEN_INVALID;
                return token;
            }
            value = value * 10U + (uint64_t)(advance(lexer) - '0');
        }
        token.kind = TOKEN_INTEGER;
        token.length =
            (size_t)(&lexer->source[lexer->offset] - token.start);
        token.integer = (uint32_t)value;
        return token;
    }

    switch (character) {
    case '(':
        token.kind = TOKEN_LEFT_PAREN;
        break;
    case ')':
        token.kind = TOKEN_RIGHT_PAREN;
        break;
    case ':':
        token.kind = TOKEN_COLON;
        break;
    case ',':
        token.kind = TOKEN_COMMA;
        break;
    case '=':
        if (peek(lexer, 0U) == '=') {
            (void)advance(lexer);
            token.length = 2U;
            token.kind = TOKEN_EQUAL_EQUAL;
        } else {
            token.kind = TOKEN_EQUAL;
        }
        break;
    case '~':
        if (peek(lexer, 0U) == '=') {
            (void)advance(lexer);
            token.length = 2U;
            token.kind = TOKEN_NOT_EQUAL;
        } else {
            token.kind = TOKEN_INVALID;
            set_diagnostic(lexer->diagnostic, token.line, token.column,
                           "expected '=' after '~'");
        }
        break;
    case '!':
        if (peek(lexer, 0U) == '=') {
            (void)advance(lexer);
            token.length = 2U;
            token.kind = TOKEN_NOT_EQUAL;
        } else {
            token.kind = TOKEN_INVALID;
            set_diagnostic(lexer->diagnostic, token.line, token.column,
                           "expected '=' after '!'");
        }
        break;
    case '<':
        if (peek(lexer, 0U) == '=') {
            (void)advance(lexer);
            token.length = 2U;
            token.kind = TOKEN_LESS_EQUAL;
        } else {
            token.kind = TOKEN_LESS;
        }
        break;
    case '>':
        if (peek(lexer, 0U) == '=') {
            (void)advance(lexer);
            token.length = 2U;
            token.kind = TOKEN_GREATER_EQUAL;
        } else {
            token.kind = TOKEN_GREATER;
        }
        break;
    case '+':
        token.kind = TOKEN_PLUS;
        break;
    case '-':
        token.kind = TOKEN_MINUS;
        break;
    case '*':
        token.kind = TOKEN_STAR;
        break;
    default:
        token.kind = TOKEN_INVALID;
        set_diagnostic(lexer->diagnostic, token.line, token.column,
                       "unexpected character '%c'", character);
        break;
    }
    return token;
}

static const char *token_name(enum token_kind kind)
{
    switch (kind) {
    case TOKEN_EOF:
        return "end of file";
    case TOKEN_IDENTIFIER:
        return "identifier";
    case TOKEN_INTEGER:
        return "integer";
    case TOKEN_FUNCTION:
        return "'function'";
    case TOKEN_LOCAL:
        return "'local'";
    case TOKEN_IF:
        return "'if'";
    case TOKEN_THEN:
        return "'then'";
    case TOKEN_ELSE:
        return "'else'";
    case TOKEN_RETURN:
        return "'return'";
    case TOKEN_END:
        return "'end'";
    case TOKEN_I32:
        return "'i32'";
    case TOKEN_BOOL:
        return "'bool'";
    case TOKEN_TRUE:
        return "'true'";
    case TOKEN_FALSE:
        return "'false'";
    case TOKEN_LEFT_PAREN:
        return "'('";
    case TOKEN_RIGHT_PAREN:
        return "')'";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_COMMA:
        return "','";
    case TOKEN_EQUAL:
        return "'='";
    case TOKEN_EQUAL_EQUAL:
        return "'=='";
    case TOKEN_NOT_EQUAL:
        return "'~='";
    case TOKEN_LESS:
        return "'<'";
    case TOKEN_LESS_EQUAL:
        return "'<='";
    case TOKEN_GREATER:
        return "'>'";
    case TOKEN_GREATER_EQUAL:
        return "'>='";
    case TOKEN_PLUS:
        return "'+'";
    case TOKEN_MINUS:
        return "'-'";
    case TOKEN_STAR:
        return "'*'";
    default:
        return "valid token";
    }
}

static void parser_advance(struct parser *parser)
{
    parser->current = next_token(&parser->lexer);
    if (parser->current.kind == TOKEN_INVALID) {
        parser->failed = 1;
    }
}

static int expect(struct parser *parser, enum token_kind kind)
{
    if (parser->failed) {
        return 0;
    }
    if (parser->current.kind != kind) {
        set_diagnostic(parser->diagnostic, parser->current.line,
                       parser->current.column, "expected %s, found %s",
                       token_name(kind), token_name(parser->current.kind));
        parser->failed = 1;
        return 0;
    }
    parser_advance(parser);
    return 1;
}

static int copy_name(struct parser *parser, char *destination,
                     const struct token *token)
{
    if (token->length > MIGA80_MAX_NAME) {
        set_diagnostic(parser->diagnostic, token->line, token->column,
                       "identifier exceeds %u characters", MIGA80_MAX_NAME);
        parser->failed = 1;
        return 0;
    }
    (void)memcpy(destination, token->start, token->length);
    destination[token->length] = '\0';
    return 1;
}

static int add_node(struct parser *parser, enum miga80_ast_kind kind,
                    unsigned int line, unsigned int column, int left,
                    int right, uint32_t value, unsigned int symbol_index,
                    enum miga80_type type)
{
    struct miga80_ast_node *node;
    const unsigned int index = parser->function->node_count;

    if (index == MIGA80_MAX_AST_NODES) {
        set_diagnostic(parser->diagnostic, line, column,
                       "expression exceeds %u AST nodes",
                       MIGA80_MAX_AST_NODES);
        parser->failed = 1;
        return MIGA80_INVALID_NODE;
    }
    node = &parser->function->nodes[index];
    node->kind = kind;
    node->line = line;
    node->column = column;
    node->left = left;
    node->right = right;
    node->value = value;
    node->symbol_index = symbol_index;
    node->type = type;
    ++parser->function->node_count;
    return (int)index;
}

static int find_parameter(const struct miga80_ast_function *function,
                          const struct token *token)
{
    unsigned int index;

    for (index = 0; index < function->parameter_count; ++index) {
        if (strlen(function->parameter_names[index]) == token->length &&
            memcmp(function->parameter_names[index], token->start,
                   token->length) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static int find_local(const struct miga80_ast_function *function,
                      const struct token *token)
{
    unsigned int index;

    for (index = 0U; index < function->local_count; ++index) {
        if (strlen(function->local_names[index]) == token->length &&
            memcmp(function->local_names[index], token->start,
                   token->length) == 0) {
            return (int)index;
        }
    }
    return -1;
}

static int parse_expression(struct parser *parser);

static const char *type_name(enum miga80_type type)
{
    return type == MIGA80_TYPE_BOOL ? "bool" : "i32";
}

static int parse_type(struct parser *parser, enum miga80_type *type)
{
    if (parser->current.kind == TOKEN_I32) {
        *type = MIGA80_TYPE_I32;
        parser_advance(parser);
        return 1;
    }
    if (parser->current.kind == TOKEN_BOOL) {
        *type = MIGA80_TYPE_BOOL;
        parser_advance(parser);
        return 1;
    }
    set_diagnostic(parser->diagnostic, parser->current.line,
                   parser->current.column, "expected scalar type, found %s",
                   token_name(parser->current.kind));
    parser->failed = 1;
    return 0;
}

static int require_type(struct parser *parser, int node_index,
                        enum miga80_type expected, unsigned int line,
                        unsigned int column, const char *context)
{
    if (node_index == MIGA80_INVALID_NODE || parser->failed) {
        return 0;
    }
    if (parser->function->nodes[node_index].type != expected) {
        set_diagnostic(parser->diagnostic, line, column,
                       "%s requires %s, found %s", context,
                       type_name(expected),
                       type_name(parser->function->nodes[node_index].type));
        parser->failed = 1;
        return 0;
    }
    return 1;
}

static int parse_primary(struct parser *parser)
{
    const struct token token = parser->current;

    if (token.kind == TOKEN_INTEGER) {
        parser_advance(parser);
        return add_node(parser, MIGA80_AST_LITERAL_I32, token.line,
                        token.column, MIGA80_INVALID_NODE,
                        MIGA80_INVALID_NODE, token.integer, 0U,
                        MIGA80_TYPE_I32);
    }
    if (token.kind == TOKEN_TRUE || token.kind == TOKEN_FALSE) {
        parser_advance(parser);
        return add_node(parser, MIGA80_AST_LITERAL_BOOL, token.line,
                        token.column, MIGA80_INVALID_NODE,
                        MIGA80_INVALID_NODE,
                        token.kind == TOKEN_TRUE ? 1U : 0U, 0U,
                        MIGA80_TYPE_BOOL);
    }
    if (token.kind == TOKEN_IDENTIFIER) {
        const int parameter = find_parameter(parser->function, &token);
        const int local = find_local(parser->function, &token);

        if (parameter < 0 && local < 0) {
            set_diagnostic(parser->diagnostic, token.line, token.column,
                           "unknown identifier '%.*s'", (int)token.length,
                           token.start);
            parser->failed = 1;
            return MIGA80_INVALID_NODE;
        }
        parser_advance(parser);
        if (parameter >= 0) {
            const enum miga80_type type =
                parser->function->parameter_types[parameter];

            return add_node(parser,
                            type == MIGA80_TYPE_BOOL
                                ? MIGA80_AST_PARAMETER_BOOL
                                : MIGA80_AST_PARAMETER_I32,
                            token.line,
                            token.column, MIGA80_INVALID_NODE,
                            MIGA80_INVALID_NODE, 0U,
                            (unsigned int)parameter, type);
        }
        return add_node(parser,
                        parser->function->local_types[local] == MIGA80_TYPE_BOOL
                            ? MIGA80_AST_LOCAL_BOOL
                            : MIGA80_AST_LOCAL_I32,
                        token.line,
                        token.column, MIGA80_INVALID_NODE,
                        MIGA80_INVALID_NODE, 0U, (unsigned int)local,
                        parser->function->local_types[local]);
    }
    if (token.kind == TOKEN_LEFT_PAREN) {
        int expression;

        parser_advance(parser);
        expression = parse_expression(parser);
        (void)expect(parser, TOKEN_RIGHT_PAREN);
        return expression;
    }

    set_diagnostic(parser->diagnostic, token.line, token.column,
                   "expected scalar expression, found %s",
                   token_name(token.kind));
    parser->failed = 1;
    return MIGA80_INVALID_NODE;
}

static int parse_unary(struct parser *parser)
{
    if (parser->current.kind == TOKEN_MINUS) {
        const struct token operation = parser->current;
        int operand;

        parser_advance(parser);
        operand = parse_unary(parser);
        if (!require_type(parser, operand, MIGA80_TYPE_I32, operation.line,
                          operation.column, "unary '-'")) {
            return MIGA80_INVALID_NODE;
        }
        return add_node(parser, MIGA80_AST_NEG_I32, operation.line,
                        operation.column, operand, MIGA80_INVALID_NODE, 0U,
                        0U, MIGA80_TYPE_I32);
    }
    return parse_primary(parser);
}

static int parse_multiply(struct parser *parser)
{
    int left = parse_unary(parser);

    while (!parser->failed && parser->current.kind == TOKEN_STAR) {
        const struct token operation = parser->current;
        int right;

        parser_advance(parser);
        right = parse_unary(parser);
        if (!require_type(parser, left, MIGA80_TYPE_I32, operation.line,
                          operation.column, "operator '*'") ||
            !require_type(parser, right, MIGA80_TYPE_I32, operation.line,
                          operation.column, "operator '*'")) {
            return MIGA80_INVALID_NODE;
        }
        left = add_node(parser, MIGA80_AST_MUL_I32, operation.line,
                        operation.column, left, right, 0U, 0U,
                        MIGA80_TYPE_I32);
    }
    return left;
}

static int parse_add(struct parser *parser)
{
    int left = parse_multiply(parser);

    while (!parser->failed &&
           (parser->current.kind == TOKEN_PLUS ||
            parser->current.kind == TOKEN_MINUS)) {
        const struct token operation = parser->current;
        const enum miga80_ast_kind kind =
            operation.kind == TOKEN_PLUS ? MIGA80_AST_ADD_I32
                                         : MIGA80_AST_SUB_I32;
        int right;

        parser_advance(parser);
        right = parse_multiply(parser);
        if (!require_type(parser, left, MIGA80_TYPE_I32, operation.line,
                          operation.column, "arithmetic operator") ||
            !require_type(parser, right, MIGA80_TYPE_I32, operation.line,
                          operation.column, "arithmetic operator")) {
            return MIGA80_INVALID_NODE;
        }
        left = add_node(parser, kind, operation.line, operation.column, left,
                        right, 0U, 0U, MIGA80_TYPE_I32);
    }
    return left;
}

static int comparison_token(enum token_kind kind)
{
    return kind == TOKEN_EQUAL_EQUAL || kind == TOKEN_NOT_EQUAL ||
           kind == TOKEN_LESS || kind == TOKEN_LESS_EQUAL ||
           kind == TOKEN_GREATER || kind == TOKEN_GREATER_EQUAL;
}

static int parse_expression(struct parser *parser)
{
    int left = parse_add(parser);

    while (!parser->failed && comparison_token(parser->current.kind)) {
        const struct token operation = parser->current;
        enum miga80_ast_kind kind = MIGA80_AST_EQ;
        int right;

        parser_advance(parser);
        right = parse_add(parser);
        if (left == MIGA80_INVALID_NODE || right == MIGA80_INVALID_NODE ||
            parser->failed) {
            return MIGA80_INVALID_NODE;
        }
        if (parser->function->nodes[left].type !=
            parser->function->nodes[right].type) {
            set_diagnostic(parser->diagnostic, operation.line,
                           operation.column,
                           "comparison operands have different types");
            parser->failed = 1;
            return MIGA80_INVALID_NODE;
        }
        if (operation.kind != TOKEN_EQUAL_EQUAL &&
            operation.kind != TOKEN_NOT_EQUAL &&
            parser->function->nodes[left].type != MIGA80_TYPE_I32) {
            set_diagnostic(parser->diagnostic, operation.line,
                           operation.column,
                           "ordered comparison requires i32 operands");
            parser->failed = 1;
            return MIGA80_INVALID_NODE;
        }
        if (operation.kind == TOKEN_NOT_EQUAL) {
            kind = MIGA80_AST_NE;
        } else if (operation.kind == TOKEN_LESS) {
            kind = MIGA80_AST_LT_I32;
        } else if (operation.kind == TOKEN_LESS_EQUAL) {
            kind = MIGA80_AST_LE_I32;
        } else if (operation.kind == TOKEN_GREATER) {
            kind = MIGA80_AST_GT_I32;
        } else if (operation.kind == TOKEN_GREATER_EQUAL) {
            kind = MIGA80_AST_GE_I32;
        }
        left = add_node(parser, kind, operation.line, operation.column, left,
                        right, 0U, 0U, MIGA80_TYPE_BOOL);
    }
    return left;
}

static int parse_parameter(struct parser *parser)
{
    const struct token name = parser->current;
    enum miga80_type type;
    unsigned int index;

    if (!expect(parser, TOKEN_IDENTIFIER)) {
        return 0;
    }
    if (parser->function->parameter_count == MIGA80_MAX_PARAMETERS) {
        set_diagnostic(parser->diagnostic, name.line, name.column,
                       "initial ABI supports at most %u parameters",
                       MIGA80_MAX_PARAMETERS);
        parser->failed = 1;
        return 0;
    }
    for (index = 0; index < parser->function->parameter_count; ++index) {
        if (strlen(parser->function->parameter_names[index]) == name.length &&
            memcmp(parser->function->parameter_names[index], name.start,
                   name.length) == 0) {
            set_diagnostic(parser->diagnostic, name.line, name.column,
                           "duplicate parameter '%.*s'", (int)name.length,
                           name.start);
            parser->failed = 1;
            return 0;
        }
    }
    index = parser->function->parameter_count;
    if (!copy_name(parser, parser->function->parameter_names[index], &name) ||
        !expect(parser, TOKEN_COLON) || !parse_type(parser, &type)) {
        return 0;
    }
    parser->function->parameter_types[index] = type;
    ++parser->function->parameter_count;
    return 1;
}

struct statement_list {
    unsigned int first;
    unsigned int last;
};

static void initialize_statement_list(struct statement_list *list)
{
    list->first = MIGA80_INVALID_STATEMENT;
    list->last = MIGA80_INVALID_STATEMENT;
}

static unsigned int allocate_statement(
    struct parser *parser, enum miga80_ast_statement_kind kind,
    unsigned int local_index, int expression, unsigned int line,
    unsigned int column)
{
    struct miga80_ast_statement *statement;
    unsigned int index;

    if (parser->function->statement_count == MIGA80_MAX_STATEMENTS) {
        set_diagnostic(parser->diagnostic, line, column,
                       "function body exceeds %u statements",
                       MIGA80_MAX_STATEMENTS);
        parser->failed = 1;
        return MIGA80_INVALID_STATEMENT;
    }
    index = parser->function->statement_count++;
    statement = &parser->function->statements[index];
    statement->kind = kind;
    statement->local_index = local_index;
    statement->expression = expression;
    statement->line = line;
    statement->column = column;
    statement->next_statement = MIGA80_INVALID_STATEMENT;
    statement->then_statement = MIGA80_INVALID_STATEMENT;
    statement->else_statement = MIGA80_INVALID_STATEMENT;
    return index;
}

static int append_statement(struct parser *parser,
                            struct statement_list *list,
                            unsigned int statement)
{
    if (statement == MIGA80_INVALID_STATEMENT) {
        return 0;
    }
    if (list->first == MIGA80_INVALID_STATEMENT) {
        list->first = statement;
    } else {
        parser->function->statements[list->last].next_statement = statement;
    }
    list->last = statement;
    return 1;
}

static int local_name_is_available(struct parser *parser,
                                   const struct token *name)
{
    if (find_parameter(parser->function, name) >= 0 ||
        find_local(parser->function, name) >= 0) {
        set_diagnostic(parser->diagnostic, name->line, name->column,
                       "duplicate local '%.*s'", (int)name->length,
                       name->start);
        parser->failed = 1;
        return 0;
    }
    return 1;
}

static unsigned int parse_local_declaration(struct parser *parser)
{
    struct token name;
    enum miga80_type type;
    int expression;
    unsigned int local_index;

    parser_advance(parser);
    name = parser->current;
    if (!expect(parser, TOKEN_IDENTIFIER)) {
        return MIGA80_INVALID_STATEMENT;
    }
    if (parser->function->local_count == MIGA80_MAX_LOCALS) {
        set_diagnostic(parser->diagnostic, name.line, name.column,
                       "function exceeds %u local variables",
                       MIGA80_MAX_LOCALS);
        parser->failed = 1;
        return MIGA80_INVALID_STATEMENT;
    }
    if (!local_name_is_available(parser, &name) ||
        !expect(parser, TOKEN_COLON) || !parse_type(parser, &type) ||
        !expect(parser, TOKEN_EQUAL)) {
        return MIGA80_INVALID_STATEMENT;
    }
    expression = parse_expression(parser);
    if (!require_type(parser, expression, type, name.line, name.column,
                      "local initializer")) {
        return MIGA80_INVALID_STATEMENT;
    }
    local_index = parser->function->local_count;
    if (!copy_name(parser, parser->function->local_names[local_index],
                   &name)) {
        return MIGA80_INVALID_STATEMENT;
    }
    parser->function->local_types[local_index] = type;
    ++parser->function->local_count;
    return allocate_statement(parser, MIGA80_AST_LOCAL_INITIALIZE,
                              local_index, expression, name.line,
                              name.column);
}

static unsigned int parse_assignment(struct parser *parser)
{
    const struct token name = parser->current;
    const int local = find_local(parser->function, &name);
    int expression;

    if (local < 0) {
        if (find_parameter(parser->function, &name) >= 0) {
            set_diagnostic(parser->diagnostic, name.line, name.column,
                           "cannot assign to parameter '%.*s'",
                           (int)name.length, name.start);
        } else {
            set_diagnostic(parser->diagnostic, name.line, name.column,
                           "unknown local assignment target '%.*s'",
                           (int)name.length, name.start);
        }
        parser->failed = 1;
        return MIGA80_INVALID_STATEMENT;
    }
    if (!expect(parser, TOKEN_IDENTIFIER) || !expect(parser, TOKEN_EQUAL)) {
        return MIGA80_INVALID_STATEMENT;
    }
    expression = parse_expression(parser);
    if (!require_type(parser, expression,
                      parser->function->local_types[local], name.line,
                      name.column, "assignment")) {
        return MIGA80_INVALID_STATEMENT;
    }
    return allocate_statement(parser, MIGA80_AST_LOCAL_ASSIGN,
                              (unsigned int)local, expression, name.line,
                              name.column);
}

static unsigned int parse_if_statement(struct parser *parser);

static int parse_conditional_statement_list(struct parser *parser,
                                            struct statement_list *list)
{
    initialize_statement_list(list);
    while (!parser->failed &&
           (parser->current.kind == TOKEN_IDENTIFIER ||
            parser->current.kind == TOKEN_IF)) {
        unsigned int statement;

        if (parser->current.kind == TOKEN_IF) {
            statement = parse_if_statement(parser);
        } else {
            statement = parse_assignment(parser);
        }
        if (!append_statement(parser, list, statement)) {
            return 0;
        }
    }
    if (parser->current.kind == TOKEN_LOCAL) {
        set_diagnostic(parser->diagnostic, parser->current.line,
                       parser->current.column,
                       "local declarations inside if are not implemented");
        parser->failed = 1;
        return 0;
    }
    if (parser->current.kind == TOKEN_RETURN) {
        set_diagnostic(parser->diagnostic, parser->current.line,
                       parser->current.column,
                       "return inside if is not implemented");
        parser->failed = 1;
        return 0;
    }
    return 1;
}

static unsigned int parse_if_statement(struct parser *parser)
{
    const struct token if_token = parser->current;
    struct statement_list then_list;
    struct statement_list else_list;
    unsigned int statement_index;
    int condition;

    parser_advance(parser);
    condition = parse_expression(parser);
    if (!require_type(parser, condition, MIGA80_TYPE_BOOL, if_token.line,
                      if_token.column, "if condition") ||
        !expect(parser, TOKEN_THEN)) {
        return MIGA80_INVALID_STATEMENT;
    }
    statement_index =
        allocate_statement(parser, MIGA80_AST_IF, 0U, condition,
                           if_token.line, if_token.column);
    if (statement_index == MIGA80_INVALID_STATEMENT ||
        !parse_conditional_statement_list(parser, &then_list) ||
        !expect(parser, TOKEN_ELSE) ||
        !parse_conditional_statement_list(parser, &else_list) ||
        !expect(parser, TOKEN_END)) {
        return MIGA80_INVALID_STATEMENT;
    }
    parser->function->statements[statement_index].then_statement =
        then_list.first;
    parser->function->statements[statement_index].else_statement =
        else_list.first;
    return statement_index;
}

int miga80_parse_function(const char *source, size_t source_size,
                          struct miga80_ast_function *function,
                          struct miga80_diagnostic *diagnostic)
{
    struct parser parser;
    struct statement_list body;
    struct token function_name;

    if (source == NULL || function == NULL || diagnostic == NULL) {
        return 0;
    }
    (void)memset(function, 0, sizeof(*function));
    (void)memset(diagnostic, 0, sizeof(*diagnostic));
    (void)memset(&parser, 0, sizeof(parser));
    parser.lexer.source = source;
    parser.lexer.size = source_size;
    parser.lexer.line = 1U;
    parser.lexer.column = 1U;
    parser.lexer.diagnostic = diagnostic;
    parser.function = function;
    parser.diagnostic = diagnostic;
    function->first_statement = MIGA80_INVALID_STATEMENT;
    initialize_statement_list(&body);
    parser_advance(&parser);

    if (!expect(&parser, TOKEN_FUNCTION)) {
        return 0;
    }
    function_name = parser.current;
    if (!expect(&parser, TOKEN_IDENTIFIER) ||
        !copy_name(&parser, function->name, &function_name) ||
        !expect(&parser, TOKEN_LEFT_PAREN)) {
        return 0;
    }
    if (parser.current.kind != TOKEN_RIGHT_PAREN) {
        if (!parse_parameter(&parser)) {
            return 0;
        }
        while (parser.current.kind == TOKEN_COMMA) {
            parser_advance(&parser);
            if (!parse_parameter(&parser)) {
                return 0;
            }
        }
    }
    if (!expect(&parser, TOKEN_RIGHT_PAREN) ||
        !expect(&parser, TOKEN_COLON) ||
        !parse_type(&parser, &function->result_type)) {
        return 0;
    }

    while (!parser.failed && (parser.current.kind == TOKEN_LOCAL ||
                              parser.current.kind == TOKEN_IDENTIFIER ||
                              parser.current.kind == TOKEN_IF)) {
        unsigned int statement;

        if (parser.current.kind == TOKEN_LOCAL) {
            statement = parse_local_declaration(&parser);
        } else if (parser.current.kind == TOKEN_IF) {
            statement = parse_if_statement(&parser);
        } else {
            statement = parse_assignment(&parser);
        }
        if (!append_statement(&parser, &body, statement)) {
            return 0;
        }
    }
    {
        const struct token return_token = parser.current;

        if (!expect(&parser, TOKEN_RETURN)) {
            return 0;
        }
        function->result = parse_expression(&parser);
        if (!require_type(&parser, function->result, function->result_type,
                          return_token.line, return_token.column,
                          "function return") ||
            !append_statement(
                &parser, &body,
                allocate_statement(&parser, MIGA80_AST_RETURN, 0U,
                                   function->result, return_token.line,
                                   return_token.column))) {
            return 0;
        }
    }
    if (parser.failed || !expect(&parser, TOKEN_END) ||
        !expect(&parser, TOKEN_EOF)) {
        return 0;
    }
    function->first_statement = body.first;
    return function->result != MIGA80_INVALID_NODE;
}
