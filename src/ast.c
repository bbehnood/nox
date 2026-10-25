#include "ast.h"

#include "lexer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define AST_PROGRAM_INITIAL_CAPACITY (8)

static ast_node_t* ast_alloc(ast_kind_t kind)
{
    ast_node_t* node = malloc(sizeof(*node));
    if (!node)
        return NULL;

    node->kind = kind;
    node->span = (source_span_t){.start = 0, .end = 0};

    return node;
}

ast_node_t* ast_integer(source_span_t span, long value)
{
    ast_node_t* node = ast_alloc(AST_INTEGER);
    if (!node)
        return NULL;

    node->span    = span;
    node->integer = value;

    return node;
}

ast_node_t* ast_identifier(source_span_t span)
{
    ast_node_t* node = ast_alloc(AST_IDENTIFIER);

    if (!node)
        return NULL;

    node->span       = span;
    node->identifier = span;

    return node;
}

ast_node_t* ast_binary(source_span_t span,
                       binary_op_t   op,
                       ast_node_t*   left,
                       ast_node_t*   right)
{
    ast_node_t* node = ast_alloc(AST_BINARY);
    if (!node)
    {
        ast_free(left);
        ast_free(right);
        return NULL;
    }

    node->span = span;

    node->binary.op    = op;
    node->binary.left  = left;
    node->binary.right = right;

    return node;
}

ast_node_t* ast_unary(source_span_t span, unary_op_t op, ast_node_t* operand)
{
    ast_node_t* node = ast_alloc(AST_UNARY);
    if (!node)
    {
        ast_free(operand);
        return NULL;
    }

    node->span          = span;
    node->unary.op      = op;
    node->unary.operand = operand;

    return node;
}

ast_node_t* ast_let(source_span_t span, source_span_t name, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_LET);

    if (node == NULL)
    {
        ast_free(value);
        return NULL;
    }

    node->span      = span;
    node->let.name  = name;
    node->let.value = value;

    return node;
}

ast_node_t* ast_print(source_span_t span, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_PRINT);

    if (node == NULL)
    {
        ast_free(value);
        return NULL;
    }

    node->span        = span;
    node->print.value = value;

    return node;
}

ast_node_t* ast_program(void)
{
    ast_node_t* node = ast_alloc(AST_PROGRAM);
    if (!node)
        return NULL;

    node->program.statements = NULL;
    node->program.count      = 0;
    node->program.capacity   = 0;

    return node;
}

bool ast_program_push(ast_node_t* program, ast_node_t* statement)
{
    if (program->program.count == program->program.capacity)
    {
        size_t new_capacity = program->program.capacity
                                  ? program->program.capacity * 2
                                  : AST_PROGRAM_INITIAL_CAPACITY;

        ast_node_t** grown =
            realloc(program->program.statements, new_capacity * sizeof(*grown));
        if (!grown)
            return false;

        program->program.statements = grown;
        program->program.capacity   = new_capacity;
    }

    program->program.statements[program->program.count++] = statement;
    return true;
}

void ast_free(ast_node_t* node)
{
    if (!node)
        return;

    switch (node->kind)
    {
    case AST_BINARY:
        ast_free(node->binary.left);
        ast_free(node->binary.right);
        break;

    case AST_UNARY:
        ast_free(node->unary.operand);
        break;

    case AST_LET:
        ast_free(node->let.value);
        break;

    case AST_PRINT:
        ast_free(node->print.value);
        break;

    case AST_INTEGER:
    case AST_IDENTIFIER:
        break;

    case AST_PROGRAM:
        for (size_t i = 0; i < node->program.count; i++)
            ast_free(node->program.statements[i]);
        free(node->program.statements);
        break;
    }

    free(node);
}
