#include "sema.h"

#include "ast.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sema_init(sema_t* sema, source_t source)
{
    sema->source     = source;
    sema->symbols    = NULL;
    sema->count      = 0;
    sema->capacity   = 0;
    sema->slot_count = 0;
    sema->failed     = false;

    sema->diagnostic.span       = (source_span_t){.start = 0, .end = 0};
    sema->diagnostic.message[0] = '\0';
}

void sema_free(sema_t* sema)
{
    free(sema->symbols);
    sema->symbols  = NULL;
    sema->count    = 0;
    sema->capacity = 0;
}

const diagnostic_t* sema_error(const sema_t* sema)
{
    return sema->failed ? &sema->diagnostic : NULL;
}

static void fail(sema_t* sema, source_span_t span, const char* fmt, ...)
{
    if (sema->failed)
        return;

    sema->failed          = true;
    sema->diagnostic.span = span;

    va_list args;
    va_start(args, fmt);
    vsnprintf(sema->diagnostic.message,
              sizeof(sema->diagnostic.message),
              fmt,
              args);
    va_end(args);
}

static bool same_name(const sema_t* sema, source_span_t a, source_span_t b)
{
    size_t length = a.end - a.start;

    return length == b.end - b.start && memcmp(sema->source.data + a.start,
                                               sema->source.data + b.start,
                                               length) == 0;
}

static const symbol_t* lookup(const sema_t* sema, source_span_t name)
{
    for (size_t i = 0; i < sema->count; i++)
    {
        if (same_name(sema, sema->symbols[i].name, name))
            return &sema->symbols[i];
    }

    return NULL;
}

static bool declare(sema_t* sema, source_span_t name, size_t* slot)
{
    if (sema->count == sema->capacity)
    {
        size_t    capacity = sema->capacity ? sema->capacity * 2 : 8;
        symbol_t* grown =
            realloc(sema->symbols, capacity * sizeof(*sema->symbols));

        if (!grown)
        {
            fail(sema, name, "out of memory");
            return false;
        }

        sema->symbols  = grown;
        sema->capacity = capacity;
    }

    *slot = sema->slot_count++;

    sema->symbols[sema->count++] = (symbol_t){.name = name, .slot = *slot};
    return true;
}

static void check_node(sema_t* sema, ast_node_t* node)
{
    if (!node || sema->failed)
        return;

    switch (node->kind)
    {
    case AST_INTEGER:
        break;

    case AST_IDENTIFIER:
    {
        const symbol_t* symbol = lookup(sema, node->identifier);

        if (!symbol)
        {
            fail(sema,
                 node->span,
                 "use of undeclared variable '%.*s'",
                 (int)(node->identifier.end - node->identifier.start),
                 sema->source.data + node->identifier.start);
            return;
        }

        node->slot = symbol->slot;
        break;
    }

    case AST_BINARY:
        check_node(sema, node->binary.left);
        check_node(sema, node->binary.right);
        break;

    case AST_UNARY:
        check_node(sema, node->unary.operand);
        break;

    case AST_PRINT:
        check_node(sema, node->print.value);
        break;

    case AST_LET:
        check_node(sema, node->let.value);
        if (sema->failed)
            return;

        if (lookup(sema, node->let.name))
        {
            fail(sema,
                 node->let.name,
                 "variable '%.*s' is already declared",
                 (int)(node->let.name.end - node->let.name.start),
                 sema->source.data + node->let.name.start);
            return;
        }

        declare(sema, node->let.name, &node->slot);
        break;

    case AST_PROGRAM:
        for (size_t i = 0; i < node->program.count; i++)
            check_node(sema, node->program.statements[i]);
        break;
    }
}

bool sema_check(sema_t* sema, ast_node_t* program)
{
    check_node(sema, program);
    return !sema->failed;
}
