#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include "lexer.h"

#include <stddef.h>
#include <stdio.h>

static constexpr int DIAGNOSTIC_MESSAGE_MAX = 160;

typedef struct diagnostic
{
    source_span_t span;
    char          message[DIAGNOSTIC_MESSAGE_MAX];
} diagnostic_t;

typedef struct source_location
{
    size_t line;
    size_t column;
    size_t line_start;
    size_t line_end;
} source_location_t;

source_location_t source_locate(source_t source, size_t offset);

void diagnostic_print(FILE*               out,
                      const char*         filename,
                      source_t            source,
                      const diagnostic_t* diagnostic);

#endif
