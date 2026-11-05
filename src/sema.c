#include "sema.h"

#include "ast.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constexpr size_t SEMA_INITIAL_SYMBOL_CAPACITY = 8;

[[gnu::format(printf, 3, 4)]]
static void sema_report(sema_t*       sema,
                        source_span_t span,
                        const char*   format,
                        ...)
{
    if (sema->failed)
        return;

    sema->failed          = true;
    sema->diagnostic.span = span;

    va_list args;
    va_start(args, format);
    vsnprintf(sema->diagnostic.message,
              sizeof(sema->diagnostic.message),
              format,
              args);
    va_end(args);
}

static bool sema_same_name(const sema_t* sema, source_span_t a, source_span_t b)
{
    size_t length = a.end - a.start;

    return length == b.end - b.start && memcmp(sema->source.data + a.start,
                                               sema->source.data + b.start,
                                               length) == 0;
}

static const symbol_t* sema_lookup_from(const sema_t* sema,
                                        source_span_t name,
                                        size_t        first)
{
    for (size_t i = sema->symbol_count; i > first; i--)
    {
        if (sema_same_name(sema, sema->symbols[i - 1].name, name))
            return &sema->symbols[i - 1];
    }

    return nullptr;
}

static const symbol_t* sema_lookup(const sema_t* sema, source_span_t name)
{
    return sema_lookup_from(sema, name, 0);
}

static const symbol_t* sema_lookup_in_scope(const sema_t* sema,
                                            source_span_t name)
{
    return sema_lookup_from(sema, name, sema->scope_start);
}

static size_t sema_enter_scope(sema_t* sema)
{
    size_t outer_start = sema->scope_start;

    sema->scope_start = sema->symbol_count;

    return outer_start;
}

static void sema_leave_scope(sema_t* sema, size_t outer_start)
{
    sema->symbol_count = sema->scope_start;
    sema->scope_start  = outer_start;
}

static bool sema_declare(sema_t*       sema,
                         source_span_t name,
                         type_t        type,
                         size_t*       slot)
{
    if (sema->symbol_count == sema->symbol_capacity)
    {
        size_t capacity = sema->symbol_capacity ? sema->symbol_capacity * 2
                                                : SEMA_INITIAL_SYMBOL_CAPACITY;
        symbol_t* grown =
            realloc(sema->symbols, capacity * sizeof(*sema->symbols));

        if (!grown)
        {
            sema_report(sema, name, "out of memory");
            return false;
        }

        sema->symbols         = grown;
        sema->symbol_capacity = capacity;
    }

    *slot = sema->slot_count++;

    sema->symbols[sema->symbol_count++] = (symbol_t){
        .name = name,
        .type = type,
        .slot = *slot,
    };
    return true;
}

static const char* type_name(type_t type)
{
    switch (type)
    {
    case TYPE_INT:
        return "int";
    case TYPE_BOOL:
        return "bool";
    case TYPE_NONE:
        break;
    }

    return "none";
}

static void sema_expect_type(sema_t*           sema,
                             const ast_node_t* node,
                             type_t            expected)
{
    if (node->type == expected)
        return;

    sema_report(sema,
                node->span,
                "expected %s, found %s",
                type_name(expected),
                type_name(node->type));
}

static void sema_check_binary(sema_t* sema, ast_node_t* node)
{
    const ast_node_t* left  = node->binary.left;
    const ast_node_t* right = node->binary.right;

    type_t result = TYPE_NONE;

    switch (node->binary.op)
    {
    case BINARY_OP_ADD:
    case BINARY_OP_SUB:
    case BINARY_OP_MUL:
    case BINARY_OP_DIV:
        sema_expect_type(sema, left, TYPE_INT);
        sema_expect_type(sema, right, TYPE_INT);
        result = TYPE_INT;
        break;

    case BINARY_OP_LT:
    case BINARY_OP_LE:
    case BINARY_OP_GT:
    case BINARY_OP_GE:
        sema_expect_type(sema, left, TYPE_INT);
        sema_expect_type(sema, right, TYPE_INT);
        result = TYPE_BOOL;
        break;

    case BINARY_OP_EQ:
    case BINARY_OP_NE:
        sema_expect_type(sema, right, left->type);
        result = TYPE_BOOL;
        break;
    }

    if (!sema->failed)
        node->type = result;
}

static void sema_check_node(sema_t* sema, ast_node_t* node)
{
    if (!node || sema->failed)
        return;

    switch (node->kind)
    {
    case AST_INTEGER:
        node->type = TYPE_INT;
        break;

    case AST_BOOLEAN:
        node->type = TYPE_BOOL;
        break;

    case AST_IDENTIFIER:
    {
        const symbol_t* symbol = sema_lookup(sema, node->identifier);

        if (!symbol)
        {
            sema_report(sema,
                        node->span,
                        "use of undeclared variable '%.*s'",
                        (int)(node->identifier.end - node->identifier.start),
                        sema->source.data + node->identifier.start);
            return;
        }

        node->slot = symbol->slot;
        node->type = symbol->type;
        break;
    }

    case AST_BINARY:
        sema_check_node(sema, node->binary.left);
        sema_check_node(sema, node->binary.right);
        if (sema->failed)
            return;

        sema_check_binary(sema, node);
        break;

    case AST_UNARY:
        sema_check_node(sema, node->unary.operand);
        if (sema->failed)
            return;

        sema_expect_type(sema, node->unary.operand, TYPE_INT);
        if (sema->failed)
            return;

        node->type = TYPE_INT;
        break;

    case AST_LET:
        sema_check_node(sema, node->let.value);
        if (sema->failed)
            return;

        if (sema_lookup_in_scope(sema, node->let.name))
        {
            sema_report(sema,
                        node->let.name,
                        "variable '%.*s' is already declared",
                        (int)(node->let.name.end - node->let.name.start),
                        sema->source.data + node->let.name.start);
            return;
        }

        sema_declare(sema, node->let.name, node->let.value->type, &node->slot);
        break;

    case AST_ASSIGN:
    {
        sema_check_node(sema, node->assign.value);
        if (sema->failed)
            return;

        const symbol_t* symbol = sema_lookup(sema, node->assign.name);

        if (!symbol)
        {
            sema_report(sema,
                        node->assign.name,
                        "assignment to undeclared variable '%.*s'",
                        (int)(node->assign.name.end - node->assign.name.start),
                        sema->source.data + node->assign.name.start);
            return;
        }

        sema_expect_type(sema, node->assign.value, symbol->type);
        if (sema->failed)
            return;

        node->slot = symbol->slot;
        break;
    }

    case AST_PRINT:
        sema_check_node(sema, node->print.value);
        break;

    case AST_BLOCK:
    {
        size_t outer_start = sema_enter_scope(sema);

        for (size_t i = 0; i < node->block.count; i++)
            sema_check_node(sema, node->block.statements[i]);

        sema_leave_scope(sema, outer_start);
        break;
    }

    case AST_PROGRAM:
        for (size_t i = 0; i < node->program.count; i++)
            sema_check_node(sema, node->program.statements[i]);
        break;
    }
}

void sema_init(sema_t* sema, source_t source)
{
    *sema = (sema_t){.source = source};
}

bool sema_check(sema_t* sema, ast_node_t* program)
{
    sema_check_node(sema, program);
    return !sema->failed;
}

const diagnostic_t* sema_error(const sema_t* sema)
{
    return sema->failed ? &sema->diagnostic : nullptr;
}

void sema_free(sema_t* sema)
{
    free(sema->symbols);
    sema->symbols         = nullptr;
    sema->symbol_count    = 0;
    sema->symbol_capacity = 0;
}
