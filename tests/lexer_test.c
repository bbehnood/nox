#include "lexer.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

static void expect_types(const char*         text,
                         const token_type_t* types,
                         size_t              count)
{
    lexer_t lexer;
    lexer_init(&lexer, SRC(text));

    for (size_t i = 0; i < count; i++)
    {
        token_t token = lexer_next(&lexer);
        if (token.type != types[i])
        {
            fprintf(stderr,
                    "  in \"%s\": token %zu: got type %d, want %d\n",
                    text,
                    i,
                    (int)token.type,
                    (int)types[i]);
        }
        ASSERT(token.type == types[i]);
    }

    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);
}

static void expect_single(const char* text, token_type_t type)
{
    expect_types(text, &type, 1);
}

static void test_empty_and_whitespace(void)
{
    lexer_t lexer;

    lexer_init(&lexer, SRC(""));
    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_EOF);
    ASSERT(t.span.start == 0 && t.span.end == 0);

    lexer_init(&lexer, SRC(" \t\r\n  "));
    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_EOF);
    ASSERT(t.span.start == 6 && t.span.end == 6);
}

static void test_eof_is_sticky(void)
{
    lexer_t lexer;
    lexer_init(&lexer, SRC("x"));

    ASSERT(lexer_next(&lexer).type == TOKEN_IDENTIFIER);
    for (int i = 0; i < 5; i++)
    {
        token_t t = lexer_next(&lexer);
        ASSERT(t.type == TOKEN_EOF);
        ASSERT(t.span.start == 1 && t.span.end == 1);
    }
}

static void test_single_char_tokens(void)
{
    expect_single("+", TOKEN_PLUS);
    expect_single("-", TOKEN_MINUS);
    expect_single("*", TOKEN_STAR);
    expect_single("/", TOKEN_SLASH);
    expect_single("=", TOKEN_EQUALS);
    expect_single("(", TOKEN_LPAREN);
    expect_single(")", TOKEN_RPAREN);
    expect_single(";", TOKEN_SEMICOLON);
}

static void test_keywords_and_identifiers(void)
{
    expect_single("let", TOKEN_LET);
    expect_single("print", TOKEN_PRINT);

    expect_single("lets", TOKEN_IDENTIFIER);
    expect_single("le", TOKEN_IDENTIFIER);
    expect_single("Let", TOKEN_IDENTIFIER);
    expect_single("LET", TOKEN_IDENTIFIER);
    expect_single("prin", TOKEN_IDENTIFIER);
    expect_single("printf", TOKEN_IDENTIFIER);
    expect_single("print_", TOKEN_IDENTIFIER);
    expect_single("let1", TOKEN_IDENTIFIER);

    expect_single("x", TOKEN_IDENTIFIER);
    expect_single("_", TOKEN_IDENTIFIER);
    expect_single("_foo", TOKEN_IDENTIFIER);
    expect_single("foo_bar9", TOKEN_IDENTIFIER);
    expect_single("a1b2", TOKEN_IDENTIFIER);
}

static void test_integers(void)
{
    lexer_t lexer;

    lexer_init(&lexer, SRC("0 42 0123"));

    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER);
    ASSERT(t.span.start == 0 && t.span.end == 1);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER);
    ASSERT(t.span.start == 2 && t.span.end == 4);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER);
    ASSERT(t.span.start == 5 && t.span.end == 9);

    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);
}

static void test_digits_then_letters_split(void)
{
    token_type_t want[] = {TOKEN_INTEGER, TOKEN_IDENTIFIER};
    expect_types("12abc", want, 2);
}

static void test_spans(void)
{
    lexer_t lexer;
    lexer_init(&lexer, SRC("let  foo = 7;"));

    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_LET);
    ASSERT(t.span.start == 0 && t.span.end == 3);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_IDENTIFIER);
    ASSERT(t.span.start == 5 && t.span.end == 8);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_EQUALS);
    ASSERT(t.span.start == 9 && t.span.end == 10);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER);
    ASSERT(t.span.start == 11 && t.span.end == 12);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_SEMICOLON);
    ASSERT(t.span.start == 12 && t.span.end == 13);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_EOF);
    ASSERT(t.span.start == 13 && t.span.end == 13);
}

static void test_statement_sequence(void)
{
    token_type_t want[] = {
        TOKEN_LET,     TOKEN_IDENTIFIER, TOKEN_EQUALS,    TOKEN_LPAREN,
        TOKEN_INTEGER, TOKEN_PLUS,       TOKEN_INTEGER,   TOKEN_RPAREN,
        TOKEN_STAR,    TOKEN_IDENTIFIER, TOKEN_SEMICOLON, TOKEN_PRINT,
        TOKEN_LPAREN,  TOKEN_IDENTIFIER, TOKEN_MINUS,     TOKEN_INTEGER,
        TOKEN_SLASH,   TOKEN_INTEGER,    TOKEN_RPAREN,    TOKEN_SEMICOLON,
    };
    expect_types("let x = (1 + 2) * y; print(x - 3 / 4);",
                 want,
                 sizeof(want) / sizeof(want[0]));
}

static void test_no_whitespace_needed(void)
{
    token_type_t want[] = {
        TOKEN_INTEGER,
        TOKEN_PLUS,
        TOKEN_INTEGER,
        TOKEN_STAR,
        TOKEN_LPAREN,
        TOKEN_IDENTIFIER,
        TOKEN_RPAREN,
    };
    expect_types("1+2*(a)", want, sizeof(want) / sizeof(want[0]));
}

static void test_comments(void)
{
    lexer_t lexer;

    expect_types("// nothing here", nullptr, 0);

    lexer_init(&lexer, SRC("1 // one\n2"));
    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER && t.span.start == 0);
    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_INTEGER && t.span.start == 9 && t.span.end == 10);
    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);

    token_type_t want[] = {TOKEN_IDENTIFIER};
    expect_types("// a\n// b\nx", want, 1);

    expect_types("/// triple", nullptr, 0);
    expect_types("// // x", nullptr, 0);

    expect_types("// c\r\nx", want, 1);
}

static void test_single_slash_is_not_comment(void)
{
    token_type_t want[] = {TOKEN_INTEGER, TOKEN_SLASH, TOKEN_INTEGER};
    expect_types("4 / 2", want, 3);

    token_type_t end[] = {TOKEN_INTEGER, TOKEN_SLASH};
    expect_types("4 /", end, 2);

    expect_single("/", TOKEN_SLASH);
}

static void test_invalid_characters(void)
{
    const char invalid[] = {'@', '#',  '$', '%', '^',  '&', '!', '~',
                            '"', '\'', '{', '}', '[',  ']', ',', '.',
                            ':', '<',  '>', '?', '\\', '|', '`'};

    for (size_t i = 0; i < sizeof(invalid); i++)
    {
        char text[2] = {invalid[i], '\0'};

        lexer_t lexer;
        lexer_init(&lexer, SRC(text));

        token_t t = lexer_next(&lexer);
        if (t.type != TOKEN_ERROR)
            fprintf(stderr,
                    "  char '%c' did not produce TOKEN_ERROR\n",
                    text[0]);
        ASSERT(t.type == TOKEN_ERROR);
        ASSERT(t.span.start == 0 && t.span.end == 1);
        ASSERT(lexer_next(&lexer).type == TOKEN_EOF);
    }
}

static void test_high_bytes_are_errors(void)
{
    const char text[] = "\xC3\xA9";
    lexer_t    lexer;
    lexer_init(&lexer, (source_t){.data = text, .length = 2});

    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_ERROR);
    ASSERT(t.span.start == 0 && t.span.end == 1);

    t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_ERROR);
    ASSERT(t.span.start == 1 && t.span.end == 2);

    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);
}

static void test_recovers_after_error(void)
{
    token_type_t want[] = {TOKEN_INTEGER, TOKEN_ERROR, TOKEN_INTEGER};
    expect_types("1 @ 2", want, 3);
}

static void test_respects_length_not_nul(void)
{
    const char text[] = "ab cd";

    lexer_t lexer;
    lexer_init(&lexer, (source_t){.data = text, .length = 2});

    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_IDENTIFIER);
    ASSERT(t.span.start == 0 && t.span.end == 2);
    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);

    const char nul[] = {'1', '\0', '2'};
    lexer_init(&lexer, (source_t){.data = nul, .length = 3});
    ASSERT(lexer_next(&lexer).type == TOKEN_INTEGER);
    ASSERT(lexer_next(&lexer).type == TOKEN_ERROR);
    ASSERT(lexer_next(&lexer).type == TOKEN_INTEGER);
    ASSERT(lexer_next(&lexer).type == TOKEN_EOF);
}

static void test_lexer_init_resets_state(void)
{
    lexer_t lexer;
    lexer_init(&lexer, SRC("a b"));
    lexer_next(&lexer);
    lexer_next(&lexer);

    lexer_init(&lexer, SRC("c"));
    token_t t = lexer_next(&lexer);
    ASSERT(t.type == TOKEN_IDENTIFIER);
    ASSERT(t.span.start == 0 && t.span.end == 1);
}

int main(void)
{
    test_empty_and_whitespace();
    test_eof_is_sticky();
    test_single_char_tokens();
    test_keywords_and_identifiers();
    test_integers();
    test_digits_then_letters_split();
    test_spans();
    test_statement_sequence();
    test_no_whitespace_needed();
    test_comments();
    test_single_slash_is_not_comment();
    test_invalid_characters();
    test_high_bytes_are_errors();
    test_recovers_after_error();
    test_respects_length_not_nul();
    test_lexer_init_resets_state();

    TEST_EXIT();
}
