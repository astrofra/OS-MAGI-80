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
    TOKEN_RETURN,
    TOKEN_END,
    TOKEN_I32,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COLON,
    TOKEN_COMMA,
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
        } else if (token_is_word(&token, "return")) {
            token.kind = TOKEN_RETURN;
        } else if (token_is_word(&token, "end")) {
            token.kind = TOKEN_END;
        } else if (token_is_word(&token, "i32")) {
            token.kind = TOKEN_I32;
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
    case TOKEN_RETURN:
        return "'return'";
    case TOKEN_END:
        return "'end'";
    case TOKEN_I32:
        return "'i32'";
    case TOKEN_LEFT_PAREN:
        return "'('";
    case TOKEN_RIGHT_PAREN:
        return "')'";
    case TOKEN_COLON:
        return "':'";
    case TOKEN_COMMA:
        return "','";
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
                    int right, uint32_t value, unsigned int parameter_index)
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
    node->parameter_index = parameter_index;
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

static int parse_expression(struct parser *parser);

static int parse_primary(struct parser *parser)
{
    const struct token token = parser->current;

    if (token.kind == TOKEN_INTEGER) {
        parser_advance(parser);
        return add_node(parser, MIGA80_AST_LITERAL_I32, token.line,
                        token.column, MIGA80_INVALID_NODE,
                        MIGA80_INVALID_NODE, token.integer, 0U);
    }
    if (token.kind == TOKEN_IDENTIFIER) {
        const int parameter = find_parameter(parser->function, &token);

        if (parameter < 0) {
            set_diagnostic(parser->diagnostic, token.line, token.column,
                           "unknown identifier '%.*s'", (int)token.length,
                           token.start);
            parser->failed = 1;
            return MIGA80_INVALID_NODE;
        }
        parser_advance(parser);
        return add_node(parser, MIGA80_AST_PARAMETER_I32, token.line,
                        token.column, MIGA80_INVALID_NODE,
                        MIGA80_INVALID_NODE, 0U, (unsigned int)parameter);
    }
    if (token.kind == TOKEN_LEFT_PAREN) {
        int expression;

        parser_advance(parser);
        expression = parse_expression(parser);
        (void)expect(parser, TOKEN_RIGHT_PAREN);
        return expression;
    }

    set_diagnostic(parser->diagnostic, token.line, token.column,
                   "expected i32 expression, found %s",
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
        return add_node(parser, MIGA80_AST_NEG_I32, operation.line,
                        operation.column, operand, MIGA80_INVALID_NODE, 0U,
                        0U);
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
        left = add_node(parser, MIGA80_AST_MUL_I32, operation.line,
                        operation.column, left, right, 0U, 0U);
    }
    return left;
}

static int parse_expression(struct parser *parser)
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
        left = add_node(parser, kind, operation.line, operation.column, left,
                        right, 0U, 0U);
    }
    return left;
}

static int parse_parameter(struct parser *parser)
{
    const struct token name = parser->current;
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
        !expect(parser, TOKEN_COLON) || !expect(parser, TOKEN_I32)) {
        return 0;
    }
    ++parser->function->parameter_count;
    return 1;
}

int miga80_parse_function(const char *source, size_t source_size,
                          struct miga80_ast_function *function,
                          struct miga80_diagnostic *diagnostic)
{
    struct parser parser;
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
        !expect(&parser, TOKEN_COLON) || !expect(&parser, TOKEN_I32) ||
        !expect(&parser, TOKEN_RETURN)) {
        return 0;
    }

    function->result = parse_expression(&parser);
    if (parser.failed || !expect(&parser, TOKEN_END) ||
        !expect(&parser, TOKEN_EOF)) {
        return 0;
    }
    return function->result != MIGA80_INVALID_NODE;
}
