#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "diagnostic.h"
#include "lexer.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct parser
{
    lexer_t lexer;

    token_t current;
    token_t previous;

    source_t source;

    bool         failed;
    size_t       depth;
    diagnostic_t diagnostic;
} parser_t;

void parser_init(parser_t* parser, source_t source);

ast_node_t* parser_parse(parser_t* parser);

const diagnostic_t* parser_error(const parser_t* parser);

#endif
