#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef struct source
{
    const char* data;
    size_t      length;
} source_t;

typedef struct source_span
{
    size_t start;
    size_t end;
} source_span_t;

typedef enum token_type
{
    TOKEN_EOF,

    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,

    TOKEN_LET,
    TOKEN_PRINT,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,

    TOKEN_EQUALS,

    TOKEN_LPAREN,
    TOKEN_RPAREN,

    TOKEN_SEMICOLON,
} token_type_t;

typedef struct token
{
    token_type_t  type;
    source_span_t span;
} token_t;

typedef struct lexer
{
    source_t source;
    size_t   position;
} lexer_t;

void lexer_init(lexer_t* lexer, source_t source);

token_t lexer_next(lexer_t* lexer);

#endif
