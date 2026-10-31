#ifndef AST_H
#define AST_H

#include "lexer.h"

#include <stddef.h>
#include <stdint.h>

typedef enum ast_kind
{
    AST_INTEGER,
    AST_IDENTIFIER,

    AST_BINARY,
    AST_UNARY,

    AST_LET,
    AST_PRINT,

    AST_PROGRAM,
} ast_kind_t;

typedef enum binary_op
{
    BINARY_OP_ADD,
    BINARY_OP_SUB,
    BINARY_OP_MUL,
    BINARY_OP_DIV,
} binary_op_t;

typedef enum unary_op
{
    UNARY_OP_NEG,
} unary_op_t;

typedef struct ast_node
{
    ast_kind_t    kind;
    source_span_t span;
    size_t        slot;

    union
    {
        int64_t integer;

        source_span_t identifier;

        struct
        {
            binary_op_t      op;
            struct ast_node* left;
            struct ast_node* right;
        } binary;

        struct
        {
            unary_op_t       op;
            struct ast_node* operand;
        } unary;

        struct
        {
            source_span_t    name;
            struct ast_node* value;
        } let;

        struct
        {
            struct ast_node* value;
        } print;

        struct
        {
            struct ast_node** statements;
            size_t            count;
            size_t            capacity;
        } program;
    };
} ast_node_t;

[[nodiscard]]
ast_node_t* ast_integer(source_span_t span, int64_t value);

[[nodiscard]]
ast_node_t* ast_identifier(source_span_t span);

[[nodiscard]]
ast_node_t* ast_binary(source_span_t span,
                       binary_op_t   op,
                       ast_node_t*   left,
                       ast_node_t*   right);

[[nodiscard]]
ast_node_t* ast_unary(source_span_t span, unary_op_t op, ast_node_t* operand);

[[nodiscard]]
ast_node_t* ast_let(source_span_t span, source_span_t name, ast_node_t* value);

[[nodiscard]]
ast_node_t* ast_print(source_span_t span, ast_node_t* value);

[[nodiscard]]
ast_node_t* ast_program(void);

[[nodiscard]]
bool ast_program_push(ast_node_t* program, ast_node_t* statement);

void ast_free(ast_node_t* node);

#endif
