#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

typedef struct parser
{
    lexer_t lexer;

    token_t current;
    token_t previous;

    source_t source;
} parser_t;

void parser_init(parser_t* parser, source_t source);

ast_node_t* parser_parse(parser_t* parser);

#endif
