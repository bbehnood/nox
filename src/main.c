#include "lexer.h"

#include <stdio.h>
#include <string.h>

static const char* token_name(token_type_t type)
{
    switch (type)
    {
    case TOKEN_EOF:
        return "EOF";
    case TOKEN_IDENTIFIER:
        return "IDENTIFIER";
    case TOKEN_INTEGER:
        return "INTEGER";
    case TOKEN_LET:
        return "LET";
    case TOKEN_PRINT:
        return "PRINT";
    case TOKEN_PLUS:
        return "PLUS";
    case TOKEN_MINUS:
        return "MINUS";
    case TOKEN_STAR:
        return "STAR";
    case TOKEN_SLASH:
        return "SLASH";
    case TOKEN_EQUALS:
        return "EQUALS";
    case TOKEN_LPAREN:
        return "LPAREN";
    case TOKEN_RPAREN:
        return "RPAREN";
    case TOKEN_SEMICOLON:
        return "SEMICOLON";
    }

    return "UNKNOWN";
}

static void print_token(token_t token, source_t source)
{
    size_t length = token.span.end - token.span.start;

    printf("%-12s [%zu, %zu)  \"",
           token_name(token.type),
           token.span.start,
           token.span.end);

    fwrite(source.data + token.span.start, 1, length, stdout);

    printf("\"\n");
}

int main(void)
{
    const char* text = "let x = 10;\n"
                       "let result = x + 20 * 3;\n"
                       "// hello\n"
                       "print(result);\n";

    source_t source = {.data = text, .length = strlen(text) - 1};

    lexer_t lexer;

    lexer_init(&lexer, source);

    for (;;)
    {
        token_t token = lexer_next(&lexer);

        print_token(token, source);

        if (token.type == TOKEN_EOF)
            break;
    }

    return 0;
}
