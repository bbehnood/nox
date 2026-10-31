#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"

#include <stddef.h>

typedef struct parser
{
    source_t source;
    lexer_t  lexer;

    token_t current;
    token_t previous;

    size_t depth;

    bool         failed;
    diagnostic_t diagnostic;
} parser_t;

void parser_init(parser_t* parser, source_t source);

[[nodiscard]]
ast_node_t* parser_parse(parser_t* parser);

const diagnostic_t* parser_error(const parser_t* parser);

#endif
