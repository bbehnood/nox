#ifndef AST_H
#define AST_H

#include "lexer.h"
typedef enum ast_kind
{
    AST_INTEGER,
    AST_IDENTIFIER,
    AST_BINARY,

    AST_LET,
    AST_PRINT,
} ast_kind_t;

typedef enum binary_op
{
    BIN_ADD,
    BIN_SUB,
    BIN_MUL,
    BIN_DIV,
} binary_op_t;

typedef struct ast_node
{
    ast_kind_t    kind;
    source_span_t span;

    union
    {
        long integer;

        source_span_t identifier;

        struct
        {
            binary_op_t      op;
            struct ast_node* left;
            struct ast_node* right;
        } binary;

        struct
        {
            source_span_t    name;
            struct ast_node* value;
        } let;

        struct
        {
            struct ast_node* value;
        } print;
    };
} ast_node_t;

ast_node_t* ast_integer(source_span_t span, long value);

ast_node_t* ast_identifier(source_span_t span);

ast_node_t* ast_binary(source_span_t span,
                       binary_op_t   op,
                       ast_node_t*   left,
                       ast_node_t*   right);

ast_node_t* ast_let(source_span_t span, source_span_t name, ast_node_t* value);

ast_node_t* ast_print(source_span_t span, ast_node_t* value);

void ast_free(ast_node_t* node);

#endif
