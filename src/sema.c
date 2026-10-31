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

static const symbol_t* sema_lookup(const sema_t* sema, source_span_t name)
{
    for (size_t i = 0; i < sema->symbol_count; i++)
    {
        if (sema_same_name(sema, sema->symbols[i].name, name))
            return &sema->symbols[i];
    }

    return nullptr;
}

static bool sema_declare(sema_t* sema, source_span_t name, size_t* slot)
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

    sema->symbols[sema->symbol_count++] =
        (symbol_t){.name = name, .slot = *slot};
    return true;
}

static void sema_check_node(sema_t* sema, ast_node_t* node)
{
    if (!node || sema->failed)
        return;

    switch (node->kind)
    {
    case AST_INTEGER:
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
        break;
    }

    case AST_BINARY:
        sema_check_node(sema, node->binary.left);
        sema_check_node(sema, node->binary.right);
        break;

    case AST_UNARY:
        sema_check_node(sema, node->unary.operand);
        break;

    case AST_LET:
        sema_check_node(sema, node->let.value);
        if (sema->failed)
            return;

        if (sema_lookup(sema, node->let.name))
        {
            sema_report(sema,
                        node->let.name,
                        "variable '%.*s' is already declared",
                        (int)(node->let.name.end - node->let.name.start),
                        sema->source.data + node->let.name.start);
            return;
        }

        sema_declare(sema, node->let.name, &node->slot);
        break;

    case AST_PRINT:
        sema_check_node(sema, node->print.value);
        break;

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
