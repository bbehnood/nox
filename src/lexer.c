#include "lexer.h"

#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>

static bool lexer_at_end(const lexer_t* lexer)
{
    return lexer->position >= lexer->source.length;
}

static char lexer_peek(const lexer_t* lexer)
{
    if (lexer_at_end(lexer))
        return '\0';

    return lexer->source.data[lexer->position];
}

static char lexer_peek_next(const lexer_t* lexer)
{
    if (lexer->position + 1 >= lexer->source.length)
        return '\0';

    return lexer->source.data[lexer->position + 1];
}

static char lexer_advance(lexer_t* lexer)
{
    if (lexer_at_end(lexer))
        return '\0';

    return lexer->source.data[lexer->position++];
}

static bool is_identifier_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_identifier_continue(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static void lexer_skip_whitespaces(lexer_t* lexer)
{
    while (!lexer_at_end(lexer))
    {
        char c = lexer_peek(lexer);

        if (isspace((unsigned char)c))
        {
            lexer_advance(lexer);
            continue;
        }

        if (c == '/' && lexer_peek_next(lexer) == '/')
        {
            lexer_advance(lexer);
            lexer_advance(lexer);

            while (!lexer_at_end(lexer) && lexer_peek(lexer) != '\n')
                lexer_advance(lexer);

            continue;
        }

        break;
    }
}

token_t make_token(token_type_t type, size_t start, size_t end)
{
    token_t token;

    token.type       = type;
    token.span.start = start;
    token.span.end   = end;

    return token;
}

static token_type_t identifier_type(const source_t* source,
                                    size_t          start,
                                    size_t          end)
{
    size_t      length = end - start;
    const char* text   = source->data + start;

    if (length == 3 && text[0] == 'l' && text[1] == 'e' && text[2] == 't')
    {
        return TOKEN_LET;
    }

    if (length == 5 && text[0] == 'p' && text[1] == 'r' && text[2] == 'i' &&
        text[3] == 'n' && text[4] == 't')
    {
        return TOKEN_PRINT;
    }

    return TOKEN_IDENTIFIER;
}

void lexer_init(lexer_t* lexer, source_t source)
{
    lexer->source   = source;
    lexer->position = 0;
}

token_t lexer_next(lexer_t* lexer)
{
    lexer_skip_whitespaces(lexer);

    size_t start = lexer->position;

    if (lexer_at_end(lexer))
        return make_token(TOKEN_EOF, start, start);

    char c = lexer_peek(lexer);

    if (is_identifier_start(c))
    {
        lexer_advance(lexer);

        while (is_identifier_continue(lexer_peek(lexer)))
        {
            lexer_advance(lexer);
        }

        size_t end = lexer->position;

        token_type_t type = identifier_type(&lexer->source, start, end);

        return make_token(type, start, end);
    }

    if (isdigit((unsigned char)c))
    {
        lexer_advance(lexer);

        while (isdigit((unsigned char)lexer_peek(lexer)))
        {
            lexer_advance(lexer);
        }

        return make_token(TOKEN_INTEGER, start, lexer->position);
    }

    lexer_advance(lexer);

    switch (c)
    {
    case '+':
        return make_token(TOKEN_PLUS, start, lexer->position);

    case '-':
        return make_token(TOKEN_MINUS, start, lexer->position);

    case '*':
        return make_token(TOKEN_STAR, start, lexer->position);

    case '/':
        return make_token(TOKEN_SLASH, start, lexer->position);

    case '=':
        return make_token(TOKEN_EQUALS, start, lexer->position);

    case '(':
        return make_token(TOKEN_LPAREN, start, lexer->position);

    case ')':
        return make_token(TOKEN_RPAREN, start, lexer->position);

    case ';':
        return make_token(TOKEN_SEMICOLON, start, lexer->position);

    default:
        return make_token(TOKEN_EOF, start, lexer->position);
    }
}
