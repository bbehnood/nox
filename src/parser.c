#include "parser.h"

#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constexpr int      DESCRIPTION_MAX     = 96;
static constexpr int      MAX_NESTING         = 256;
static constexpr int      MAX_CHAIN           = 4096;
static constexpr int      TOKEN_TEXT_PREVIEW  = 24;
static constexpr uint64_t INT64_MIN_MAGNITUDE = (uint64_t)INT64_MAX + 1;

static_assert(INT64_MIN_MAGNITUDE == UINT64_C(1) << 63);

static ast_node_t* parser_parse_expression(parser_t* parser);

static void parser_advance(parser_t* parser)
{
    parser->previous = parser->current;
    parser->current  = lexer_next(&parser->lexer);
}

static bool parser_check(const parser_t* parser, token_type_t type)
{
    return parser->current.type == type;
}

static bool parser_match(parser_t* parser, token_type_t type)
{
    if (!parser_check(parser, type))
        return false;

    parser_advance(parser);
    return true;
}

[[gnu::format(printf, 3, 4)]]
static void parser_report(parser_t*     parser,
                          source_span_t span,
                          const char*   format,
                          ...)
{
    if (parser->failed)
        return;

    parser->failed          = true;
    parser->diagnostic.span = span;

    va_list args;
    va_start(args, format);
    vsnprintf(parser->diagnostic.message,
              sizeof(parser->diagnostic.message),
              format,
              args);
    va_end(args);
}

static const char* token_symbol(token_type_t type)
{
    switch (type)
    {
    case TOKEN_LET:
        return "'let'";
    case TOKEN_PRINT:
        return "'print'";
    case TOKEN_PLUS:
        return "'+'";
    case TOKEN_MINUS:
        return "'-'";
    case TOKEN_STAR:
        return "'*'";
    case TOKEN_SLASH:
        return "'/'";
    case TOKEN_EQUALS:
        return "'='";
    case TOKEN_LPAREN:
        return "'('";
    case TOKEN_RPAREN:
        return "')'";
    case TOKEN_SEMICOLON:
        return "';'";
    case TOKEN_EOF:
    case TOKEN_ERROR:
    case TOKEN_IDENTIFIER:
    case TOKEN_INTEGER:
        break;
    }

    return "token";
}

static void describe_token(const parser_t* parser,
                           token_t         token,
                           char*           out,
                           size_t          size)
{
    size_t      length = token.span.end - token.span.start;
    const char* text   = parser->source.data + token.span.start;

    switch (token.type)
    {
    case TOKEN_EOF:
        snprintf(out, size, "end of input");
        return;

    case TOKEN_ERROR:
    {
        unsigned char c = (unsigned char)text[0];

        if (isprint(c))
            snprintf(out, size, "invalid character '%c'", c);
        else
            snprintf(out, size, "invalid byte 0x%02X", (unsigned)c);
        return;
    }

    case TOKEN_IDENTIFIER:
    case TOKEN_INTEGER:
    {
        const char* kind =
            token.type == TOKEN_IDENTIFIER ? "identifier" : "number";
        bool clipped = length > TOKEN_TEXT_PREVIEW;
        int  shown   = (int)(clipped ? TOKEN_TEXT_PREVIEW : length);

        snprintf(out,
                 size,
                 "%s '%.*s%s'",
                 kind,
                 shown,
                 text,
                 clipped ? "..." : "");
        return;
    }

    default:
        snprintf(out, size, "%s", token_symbol(token.type));
        return;
    }
}

static void parser_fail_expected(parser_t* parser, const char* expected)
{
    token_t       token = parser->current;
    source_span_t span  = token.span;
    char          found[DESCRIPTION_MAX];

    describe_token(parser, token, found, sizeof(found));

    if (token.type == TOKEN_ERROR)
    {
        parser_report(parser, span, "%s", found);
        return;
    }

    if (token.type == TOKEN_EOF)
    {
        span.start = parser->previous.span.end;
        span.end   = span.start;
    }

    parser_report(parser, span, "expected %s, found %s", expected, found);
}

[[nodiscard]]
static bool parser_expect(parser_t*    parser,
                          token_type_t type,
                          const char*  expected)
{
    if (!parser_check(parser, type))
    {
        parser_fail_expected(parser, expected);
        return false;
    }

    parser_advance(parser);
    return true;
}

[[nodiscard]]
static bool parser_enter(parser_t* parser)
{
    if (parser->depth >= MAX_NESTING)
    {
        parser_report(parser,
                      parser->current.span,
                      "expression is nested too deeply (limit is %d)",
                      MAX_NESTING);

        return false;
    }

    parser->depth++;
    return true;
}

static void parser_leave(parser_t* parser)
{
    parser->depth--;
}

[[nodiscard]]
static bool parse_magnitude(const parser_t* parser,
                            token_t         token,
                            uint64_t*       out)
{
    uint64_t value = 0;

    for (size_t i = token.span.start; i < token.span.end; i++)
    {
        unsigned digit = (unsigned)(parser->source.data[i] - '0');

        if (value > (UINT64_MAX - digit) / 10)
            return false;

        value = value * 10 + digit;
    }

    *out = value;
    return true;
}

static ast_node_t* parser_parse_integer(parser_t* parser,
                                        token_t   token,
                                        bool      negated)
{
    uint64_t magnitude;
    uint64_t limit = negated ? INT64_MIN_MAGNITUDE : (uint64_t)INT64_MAX;

    if (!parse_magnitude(parser, token, &magnitude) || magnitude > limit)
    {
        parser_report(parser,
                      token.span,
                      "integer literal is too large (maximum is %" PRId64 ")",
                      (int64_t)INT64_MAX);
        return nullptr;
    }

    int64_t value =
        magnitude == INT64_MIN_MAGNITUDE ? INT64_MIN : (int64_t)magnitude;

    return ast_integer(token.span, value);
}

static ast_node_t* parser_parse_primary(parser_t* parser)
{
    token_t token = parser->current;

    if (parser_match(parser, TOKEN_INTEGER))
        return parser_parse_integer(parser, token, false);

    if (parser_match(parser, TOKEN_IDENTIFIER))
        return ast_identifier(token.span);

    if (parser_match(parser, TOKEN_LPAREN))
    {
        if (!parser_enter(parser))
            return nullptr;
        ast_node_t* expression = parser_parse_expression(parser);
        parser_leave(parser);

        if (!expression)
            return nullptr;

        if (!parser_expect(parser, TOKEN_RPAREN, "')' to close the '('"))
        {
            ast_free(expression);
            return nullptr;
        }

        return expression;
    }

    parser_fail_expected(parser, "an expression");

    return nullptr;
}

static ast_node_t* parser_parse_unary(parser_t* parser)
{
    if (parser_match(parser, TOKEN_MINUS))
    {
        size_t start = parser->previous.span.start;

        if (parser_check(parser, TOKEN_INTEGER))
        {
            token_t  literal = parser->current;
            uint64_t magnitude;

            if (parse_magnitude(parser, literal, &magnitude) &&
                magnitude == INT64_MIN_MAGNITUDE)
            {
                parser_advance(parser);

                ast_node_t* node = parser_parse_integer(parser, literal, true);
                if (node)
                    node->span.start = start;
                return node;
            }
        }

        if (!parser_enter(parser))
            return nullptr;
        ast_node_t* operand = parser_parse_unary(parser);
        parser_leave(parser);

        if (!operand)
            return nullptr;

        source_span_t span = {.start = start, .end = operand->span.end};
        return ast_unary(span, UN_NEG, operand);
    }

    return parser_parse_primary(parser);
}

static ast_node_t* parser_parse_multiplicative(parser_t* parser)
{
    ast_node_t* left = parser_parse_unary(parser);
    if (!left)
        return nullptr;

    size_t chain = 0;

    for (;;)
    {
        binary_op_t op;

        if (parser_match(parser, TOKEN_STAR))
            op = BIN_MUL;
        else if (parser_match(parser, TOKEN_SLASH))
            op = BIN_DIV;
        else
            break;

        if (++chain > MAX_CHAIN)
        {
            parser_report(parser,
                          parser->previous.span,
                          "too many chained operators (limit is %d)",
                          MAX_CHAIN);

            ast_free(left);
            return nullptr;
        }

        ast_node_t* right = parser_parse_unary(parser);
        if (!right)
        {
            ast_free(left);
            return nullptr;
        }

        source_span_t span = {
            .start = left->span.start,
            .end   = right->span.end,
        };

        ast_node_t* binary = ast_binary(span, op, left, right);
        if (!binary)
            return nullptr;

        left = binary;
    }

    return left;
}

static ast_node_t* parser_parse_additive(parser_t* parser)
{
    ast_node_t* left = parser_parse_multiplicative(parser);
    if (!left)
        return nullptr;

    size_t chain = 0;

    for (;;)
    {
        binary_op_t op;

        if (parser_match(parser, TOKEN_PLUS))
            op = BIN_ADD;
        else if (parser_match(parser, TOKEN_MINUS))
            op = BIN_SUB;
        else
            break;

        if (++chain > MAX_CHAIN)
        {
            parser_report(parser,
                          parser->previous.span,
                          "too many chained operators (limit is %d)",
                          MAX_CHAIN);

            ast_free(left);
            return nullptr;
        }

        ast_node_t* right = parser_parse_multiplicative(parser);
        if (!right)
        {
            ast_free(left);
            return nullptr;
        }

        source_span_t span = {
            .start = left->span.start,
            .end   = right->span.end,
        };

        ast_node_t* binary = ast_binary(span, op, left, right);
        if (!binary)
            return nullptr;

        left = binary;
    }

    return left;
}

static ast_node_t* parser_parse_expression(parser_t* parser)
{
    return parser_parse_additive(parser);
}

static ast_node_t* parser_parse_let(parser_t* parser)
{
    size_t start = parser->previous.span.start;

    if (!parser_check(parser, TOKEN_IDENTIFIER))
    {
        parser_fail_expected(parser, "a variable name after 'let'");
        return nullptr;
    }

    token_t name = parser->current;

    parser_advance(parser);

    if (!parser_expect(parser, TOKEN_EQUALS, "'=' after the variable name"))
        return nullptr;

    ast_node_t* value = parser_parse_expression(parser);
    if (!value)
        return nullptr;

    if (!parser_expect(parser, TOKEN_SEMICOLON, "';' after the let statement"))
    {
        ast_free(value);
        return nullptr;
    }

    source_span_t span = {
        .start = start,
        .end   = parser->previous.span.end,
    };

    return ast_let(span, name.span, value);
}

static ast_node_t* parser_parse_print(parser_t* parser)
{
    size_t start = parser->previous.span.start;

    if (!parser_expect(parser, TOKEN_LPAREN, "'(' after 'print'"))
        return nullptr;

    ast_node_t* value = parser_parse_expression(parser);
    if (!value)
        return nullptr;

    if (!parser_expect(parser, TOKEN_RPAREN, "')' after the expression"))
    {
        ast_free(value);
        return nullptr;
    }

    if (!parser_expect(parser,
                       TOKEN_SEMICOLON,
                       "';' after the print statement"))
    {
        ast_free(value);
        return nullptr;
    }

    source_span_t span = {
        .start = start,
        .end   = parser->previous.span.end,
    };

    return ast_print(span, value);
}

static ast_node_t* parser_parse_statement(parser_t* parser)
{
    if (parser_match(parser, TOKEN_LET))
    {
        return parser_parse_let(parser);
    }

    if (parser_match(parser, TOKEN_PRINT))
    {
        return parser_parse_print(parser);
    }

    parser_fail_expected(parser, "a statement ('let' or 'print')");

    return nullptr;
}

void parser_init(parser_t* parser, source_t source)
{
    *parser = (parser_t){
        .source   = source,
        .previous = {.type = TOKEN_EOF},
    };

    lexer_init(&parser->lexer, source);

    parser->current = lexer_next(&parser->lexer);
}

ast_node_t* parser_parse(parser_t* parser)
{
    ast_node_t* program = ast_program();
    if (!program)
    {
        parser_report(parser, parser->current.span, "out of memory");
        return nullptr;
    }

    while (!parser_check(parser, TOKEN_EOF))
    {
        ast_node_t* statement = parser_parse_statement(parser);
        if (!statement || !ast_program_push(program, statement))
        {
            parser_report(parser, parser->current.span, "out of memory");

            ast_free(statement);
            ast_free(program);
            return nullptr;
        }
    }

    program->span.start = 0;
    program->span.end   = parser->source.length;

    return program;
}

const diagnostic_t* parser_error(const parser_t* parser)
{
    return parser->failed ? &parser->diagnostic : nullptr;
}
