#include "ast.h"

#include "lexer.h"

#include <stdlib.h>
#include <sys/types.h>

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
        return NULL;

    node->span = span;

    node->binary.op    = op;
    node->binary.left  = left;
    node->binary.right = right;

    return node;
}

ast_node_t* ast_let(source_span_t span, source_span_t name, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_LET);

    if (node == NULL)
        return NULL;

    node->span      = span;
    node->let.name  = name;
    node->let.value = value;

    return node;
}

ast_node_t* ast_print(source_span_t span, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_PRINT);

    if (node == NULL)
        return NULL;

    node->span        = span;
    node->print.value = value;

    return node;
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

    case AST_LET:
        ast_free(node->let.value);
        break;

    case AST_PRINT:
        ast_free(node->print.value);
        break;

    case AST_INTEGER:
    case AST_IDENTIFIER:
        break;
    }

    free(node);
}
