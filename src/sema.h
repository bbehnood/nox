#ifndef SEMA_H
#define SEMA_H

#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"

#include <stddef.h>

typedef struct symbol
{
    source_span_t name;
    size_t        slot;
} symbol_t;

typedef struct sema
{
    source_t source;

    symbol_t* symbols;
    size_t    count;
    size_t    capacity;

    size_t slot_count;

    bool         failed;
    diagnostic_t diagnostic;
} sema_t;

void sema_init(sema_t* sema, source_t source);

[[nodiscard]]
bool sema_check(sema_t* sema, ast_node_t* program);

const diagnostic_t* sema_error(const sema_t* sema);

void sema_free(sema_t* sema);

#endif
