#include "ast.h"

#include "lexer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static constexpr size_t AST_STATEMENT_LIST_INITIAL_CAPACITY = 8;

static ast_node_t* ast_alloc(ast_kind_t kind)
{
    ast_node_t* node = malloc(sizeof(*node));
    if (!node)
        return nullptr;

    node->kind = kind;
    node->span = (source_span_t){};
    node->slot = 0;

    return node;
}

ast_node_t* ast_integer(source_span_t span, int64_t value)
{
    ast_node_t* node = ast_alloc(AST_INTEGER);
    if (!node)
        return nullptr;

    node->span    = span;
    node->integer = value;

    return node;
}

ast_node_t* ast_identifier(source_span_t span)
{
    ast_node_t* node = ast_alloc(AST_IDENTIFIER);
    if (!node)
        return nullptr;

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
        return nullptr;
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
        return nullptr;
    }

    node->span          = span;
    node->unary.op      = op;
    node->unary.operand = operand;

    return node;
}

ast_node_t* ast_let(source_span_t span, source_span_t name, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_LET);
    if (!node)
    {
        ast_free(value);
        return nullptr;
    }

    node->span      = span;
    node->let.name  = name;
    node->let.value = value;

    return node;
}

ast_node_t* ast_assign(source_span_t span,
                       source_span_t name,
                       ast_node_t*   value)
{
    ast_node_t* node = ast_alloc(AST_ASSIGN);
    if (!node)
    {
        ast_free(value);
        return nullptr;
    }

    node->span         = span;
    node->assign.name  = name;
    node->assign.value = value;

    return node;
}

ast_node_t* ast_print(source_span_t span, ast_node_t* value)
{
    ast_node_t* node = ast_alloc(AST_PRINT);
    if (!node)
    {
        ast_free(value);
        return nullptr;
    }

    node->span        = span;
    node->print.value = value;

    return node;
}

ast_node_t* ast_block(void)
{
    ast_node_t* node = ast_alloc(AST_BLOCK);
    if (!node)
        return nullptr;

    node->block = (ast_statement_list_t){};

    return node;
}

ast_node_t* ast_program(void)
{
    ast_node_t* node = ast_alloc(AST_PROGRAM);
    if (!node)
        return nullptr;

    node->program = (ast_statement_list_t){};

    return node;
}

bool ast_statement_list_push(ast_statement_list_t* list, ast_node_t* statement)
{
    if (list->count == list->capacity)
    {
        size_t new_capacity = list->capacity
                                  ? list->capacity * 2
                                  : AST_STATEMENT_LIST_INITIAL_CAPACITY;

        ast_node_t** grown =
            realloc(list->statements, new_capacity * sizeof(*grown));
        if (!grown)
            return false;

        list->statements = grown;
        list->capacity   = new_capacity;
    }

    list->statements[list->count++] = statement;
    return true;
}

static void ast_statement_list_free(ast_statement_list_t* list)
{
    for (size_t i = 0; i < list->count; i++)
        ast_free(list->statements[i]);

    free(list->statements);
}

void ast_free(ast_node_t* node)
{
    if (!node)
        return;

    switch (node->kind)
    {
    case AST_INTEGER:
    case AST_IDENTIFIER:
        break;

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

    case AST_ASSIGN:
        ast_free(node->assign.value);
        break;

    case AST_PRINT:
        ast_free(node->print.value);
        break;

    case AST_BLOCK:
        ast_statement_list_free(&node->block);
        break;

    case AST_PROGRAM:
        ast_statement_list_free(&node->program);
        break;
    }

    free(node);
}
