#include "parser.h"

#include "ast.h"
#include "lexer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ast_node_t* parse_expression(parser_t* parser);

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

static bool parser_expect(parser_t* parser, token_type_t type)
{
    if (!parser_check(parser, type))
    {
        fprintf(stderr, "parser error: unexpected token");
        return false;
    }

    parser_advance(parser);
    return true;
}

static ast_node_t* parse_primary(parser_t* parser)
{
    token_t token = parser->current;

    if (parser_match(parser, TOKEN_INTEGER))
    {
        size_t length = token.span.end - token.span.start;

        char buffer[64];

        if (length >= sizeof(buffer))
        {
            fprintf(stderr, "parser error: integer literal too long\n");
            return NULL;
        }

        memcpy(buffer, parser->source.data, length);

        buffer[length] = '\0';

        char* end = NULL;

        long value = strtol(buffer, &end, 10);

        if (*end != '\0')
        {
            fprintf(stderr, "parser error: invalid integer\n");
            return NULL;
        }

        return ast_integer(token.span, value);
    }

    if (parser_match(parser, TOKEN_IDENTIFIER))
    {
        return ast_identifier(token.span);
    }

    if (parser_match(parser, TOKEN_LPAREN))
    {
        ast_node_t* expression = parse_expression(parser);

        if (!expression)
            return NULL;

        if (!parser_expect(parser, TOKEN_RPAREN))
        {
            ast_free(expression);
            return NULL;
        }

        return expression;
    }

    fprintf(stderr, "parser error: expected expression");

    return NULL;
}

static ast_node_t* parse_multiplicative(parser_t* parser)
{
    ast_node_t* left = parse_primary(parser);
    if (!left)
        return NULL;

    for (;;)
    {
        binary_op_t op;

        if (parser_match(parser, TOKEN_STAR))
            op = BIN_MUL;
        else if (parser_match(parser, TOKEN_SLASH))
            op = BIN_DIV;
        else
            break;

        ast_node_t* right = parse_primary(parser);
        if (!right)
        {
            ast_free(left);
            return NULL;
        }

        source_span_t span = {
            .start = left->span.start,
            .end   = right->span.end,
        };

        ast_node_t* binary = ast_binary(span, op, left, right);
        if (!binary)
        {
            ast_free(left);
            ast_free(right);
            return NULL;
        }

        left = binary;
    }

    return left;
}

static ast_node_t* parse_additive(parser_t* parser)
{
    ast_node_t* left = parse_multiplicative(parser);
    if (!left)
        return NULL;

    for (;;)
    {
        binary_op_t op;

        if (parser_match(parser, TOKEN_PLUS))
            op = BIN_ADD;
        else if (parser_match(parser, TOKEN_MINUS))
            op = BIN_SUB;
        else
            break;

        ast_node_t* right = parse_multiplicative(parser);
        if (!right)
        {
            ast_free(left);
            return NULL;
        }

        source_span_t span = {
            .start = left->span.start,
            .end   = right->span.end,
        };

        ast_node_t* binary = ast_binary(span, op, left, right);
        if (!binary)
        {
            ast_free(left);
            ast_free(right);
            return NULL;
        }

        left = binary;
    }

    return left;
}

static ast_node_t* parse_expression(parser_t* parser)
{
    return parse_additive(parser);
}

static ast_node_t* parse_let(parser_t* parser)
{
    if (!parser_check(parser, TOKEN_IDENTIFIER))
    {
        fprintf(stderr, "parser error: expected identifier after 'let'\n");
        return NULL;
    }

    token_t name = parser->current;

    parser_advance(parser);

    if (!parser_expect(parser, TOKEN_EQUALS))
        return NULL;

    ast_node_t* value = parse_expression(parser);
    if (!value)
        return NULL;

    if (!parser_expect(parser, TOKEN_SEMICOLON))
    {
        ast_free(value);
        return NULL;
    }

    source_span_t span = {
        .start = name.span.start,
        .end   = parser->previous.span.end,
    };

    return ast_let(span, name.span, value);
}

static ast_node_t* parse_print(parser_t* parser)
{
    size_t start = parser->previous.span.start;

    if (!parser_expect(parser, TOKEN_LPAREN))
        return NULL;

    ast_node_t* value = parse_expression(parser);
    if (!value)
        return NULL;

    if (!parser_expect(parser, TOKEN_RPAREN))
    {
        ast_free(value);
        return NULL;
    }

    if (!parser_expect(parser, TOKEN_SEMICOLON))
    {
        ast_free(value);
        return NULL;
    }

    source_span_t span = {
        .start = start,
        .end   = parser->previous.span.end,
    };

    return ast_print(span, value);
}

ast_node_t* parser_parse_statement(parser_t* parser)
{
    if (parser_match(parser, TOKEN_LET))
    {
        return parse_let(parser);
    }

    if (parser_match(parser, TOKEN_PRINT))
    {
        return parse_print(parser);
    }

    fprintf(stderr, "parser error: expected statement\n");

    return NULL;
}

void parser_init(parser_t* parser, source_t source)
{
    parser->source = source;

    lexer_init(&parser->lexer, source);

    parser->current = lexer_next(&parser->lexer);

    parser->previous = (token_t){
        .type = TOKEN_EOF,
        .span =
            {
                .start = 0,
                .end   = 0,
            },
    };
}
