#include "ast.h"
#include "parser.h"
#include "test.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Parse `text`. Caller owns the result (may be NULL on parse error). */
static ast_node_t* parse(const char* text)
{
    parser_t parser;
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});
    return parser_parse(&parser);
}

/* Parse text expected to be valid; returns the program node. */
static ast_node_t* parse_ok(const char* text)
{
    ast_node_t* program = parse(text);
    if (!program)
        fprintf(stderr, "  unexpected parse failure for \"%s\"\n", text);
    ASSERT(program != NULL);
    return program;
}

static void expect_fail(const char* text)
{
    fprintf(stderr, "  [expected error] ");
    ast_node_t* program = parse(text);
    if (program)
        fprintf(stderr, "  \"%s\" should not have parsed\n", text);
    ASSERT(program == NULL);
    ast_free(program);
}

/* Single statement `print(<expr>);` -> returns the expression node, owned by
 * `program`. */
static ast_node_t* expr_of(ast_node_t* program)
{
    ASSERT(program && program->kind == AST_PROGRAM);
    ASSERT(program->program.count == 1);
    ast_node_t* stmt = program->program.statements[0];
    ASSERT(stmt->kind == AST_PRINT);
    return stmt->print.value;
}

static bool is_int(const ast_node_t* n, long v)
{
    return n && n->kind == AST_INTEGER && n->integer == v;
}

static bool is_bin(const ast_node_t* n, binary_op_t op)
{
    return n && n->kind == AST_BINARY && n->binary.op == op;
}

/* ---- ast.c ------------------------------------------------------------- */

static void test_ast_constructors(void)
{
    source_span_t sp = {.start = 3, .end = 5};

    ast_node_t* i = ast_integer(sp, 99);
    ASSERT(i && i->kind == AST_INTEGER && i->integer == 99);
    ASSERT(i->span.start == 3 && i->span.end == 5);

    ast_node_t* id = ast_identifier(sp);
    ASSERT(id && id->kind == AST_IDENTIFIER);
    ASSERT(id->identifier.start == 3 && id->identifier.end == 5);

    ast_node_t* b = ast_binary(sp, BIN_SUB, i, id);
    ASSERT(b && b->kind == AST_BINARY && b->binary.op == BIN_SUB);
    ASSERT(b->binary.left == i && b->binary.right == id);

    ast_node_t* l = ast_let(sp, (source_span_t){.start = 0, .end = 1}, b);
    ASSERT(l && l->kind == AST_LET && l->let.value == b);
    ASSERT(l->let.name.start == 0 && l->let.name.end == 1);

    ast_node_t* p = ast_print(sp, ast_integer(sp, 1));
    ASSERT(p && p->kind == AST_PRINT && is_int(p->print.value, 1));

    ast_free(l); /* frees b, i, id recursively */
    ast_free(p);
}

static void test_ast_free_null(void)
{
    ast_free(NULL); /* must be a no-op */
    ASSERT(1);
}

static void test_ast_program_push_growth(void)
{
    ast_node_t* program = ast_program();
    ASSERT(program && program->kind == AST_PROGRAM);
    ASSERT(program->program.count == 0);
    ASSERT(program->program.statements == NULL);

    /* Crosses several capacity doublings (8 -> 16 -> 32 -> 64). */
    for (long i = 0; i < 50; i++)
    {
        ast_node_t* n = ast_integer((source_span_t){0, 0}, i);
        ASSERT(n != NULL);
        ASSERT(ast_program_push(program, n));
    }

    ASSERT(program->program.count == 50);
    ASSERT(program->program.capacity >= 50);

    for (long i = 0; i < 50; i++)
        ASSERT(is_int(program->program.statements[i], i));

    ast_free(program);
}

/* ---- parsing: valid programs ------------------------------------------- */

static void test_empty_program(void)
{
    ast_node_t* p = parse_ok("");
    ASSERT(p && p->kind == AST_PROGRAM && p->program.count == 0);
    ASSERT(p->span.start == 0 && p->span.end == 0);
    ast_free(p);

    p = parse_ok("  // only a comment\n\n");
    ASSERT(p && p->program.count == 0);
    ast_free(p);
}

static void test_let_statement(void)
{
    const char* src = "let x = 5;";
    ast_node_t* p   = parse_ok(src);

    ASSERT(p && p->program.count == 1);
    ASSERT(p->span.start == 0 && p->span.end == strlen(src));

    ast_node_t* s = p->program.statements[0];
    ASSERT(s->kind == AST_LET);
    ASSERT(s->let.name.start == 4 && s->let.name.end == 5);
    ASSERT(is_int(s->let.value, 5));
    /* Span covers the whole statement: `let` keyword through semicolon. */
    ASSERT(s->span.start == 0 && s->span.end == strlen(src));

    ast_free(p);
}

static void test_let_span_with_leading_text(void)
{
    /* The span must start at the `let` keyword, not at the variable name. */
    const char* src = "  // c\n  let   foo = 1 + 2 ;";
    ast_node_t* p   = parse_ok(src);

    ASSERT(p && p->program.count == 1);
    ast_node_t* s = p->program.statements[0];
    ASSERT(s->kind == AST_LET);
    ASSERT(s->span.start == 9);                /* 'l' of let */
    ASSERT(s->span.end == strlen(src));        /* after ';' */
    ASSERT(s->let.name.start == 15 && s->let.name.end == 18);

    ast_free(p);
}

static void test_statement_spans_in_sequence(void)
{
    const char* src = "let a = 1; print(a); let b = 2;";
    ast_node_t* p   = parse_ok(src);

    ASSERT(p && p->program.count == 3);
    ast_node_t** st = p->program.statements;
    ASSERT(st[0]->span.start == 0 && st[0]->span.end == 10);
    ASSERT(st[1]->span.start == 11 && st[1]->span.end == 20);
    ASSERT(st[2]->span.start == 21 && st[2]->span.end == 31);

    ast_free(p);
}

static void test_print_statement(void)
{
    const char* src = "print(7);";
    ast_node_t* p   = parse_ok(src);

    ASSERT(p && p->program.count == 1);
    ast_node_t* s = p->program.statements[0];
    ASSERT(s->kind == AST_PRINT);
    ASSERT(is_int(s->print.value, 7));
    ASSERT(s->span.start == 0 && s->span.end == strlen(src));

    ast_free(p);
}

static void test_multiple_statements(void)
{
    ast_node_t* p = parse_ok("let a = 1;\nlet b = a + 2;\nprint(b);");

    ASSERT(p && p->program.count == 3);
    ASSERT(p->program.statements[0]->kind == AST_LET);
    ASSERT(p->program.statements[1]->kind == AST_LET);
    ASSERT(p->program.statements[2]->kind == AST_PRINT);

    ast_free(p);
}

static void test_many_statements(void)
{
    /* Exceed the initial program capacity through the real parser. */
    char   buf[2048];
    size_t len = 0;
    for (int i = 0; i < 40; i++)
        len += (size_t)snprintf(buf + len, sizeof(buf) - len, "print(%d);", i);

    ast_node_t* p = parse_ok(buf);
    ASSERT(p && p->program.count == 40);
    for (long i = 0; i < 40; i++)
        ASSERT(is_int(p->program.statements[i]->print.value, i));
    ast_free(p);
}

static void test_identifier_expression(void)
{
    const char* src = "print(foo);";
    ast_node_t* p   = parse_ok(src);
    ast_node_t* e   = expr_of(p);

    ASSERT(e->kind == AST_IDENTIFIER);
    ASSERT(e->identifier.start == 6 && e->identifier.end == 9);

    ast_free(p);
}

static void test_precedence(void)
{
    /* 1 + 2 * 3  ==>  1 + (2 * 3) */
    ast_node_t* p = parse_ok("print(1 + 2 * 3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_ADD));
    ASSERT(is_int(e->binary.left, 1));
    ASSERT(is_bin(e->binary.right, BIN_MUL));
    ASSERT(is_int(e->binary.right->binary.left, 2));
    ASSERT(is_int(e->binary.right->binary.right, 3));
    ast_free(p);

    /* 1 * 2 + 3  ==>  (1 * 2) + 3 */
    p = parse_ok("print(1 * 2 + 3);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_ADD));
    ASSERT(is_bin(e->binary.left, BIN_MUL));
    ASSERT(is_int(e->binary.right, 3));
    ast_free(p);
}

static void test_left_associativity(void)
{
    /* 10 - 3 - 2  ==>  (10 - 3) - 2 */
    ast_node_t* p = parse_ok("print(10 - 3 - 2);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_SUB));
    ASSERT(is_int(e->binary.right, 2));
    ASSERT(is_bin(e->binary.left, BIN_SUB));
    ASSERT(is_int(e->binary.left->binary.left, 10));
    ASSERT(is_int(e->binary.left->binary.right, 3));
    ast_free(p);

    /* 8 / 4 / 2  ==>  (8 / 4) / 2 */
    p = parse_ok("print(8 / 4 / 2);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_DIV));
    ASSERT(is_int(e->binary.right, 2));
    ASSERT(is_bin(e->binary.left, BIN_DIV));
    ASSERT(is_int(e->binary.left->binary.left, 8));
    ASSERT(is_int(e->binary.left->binary.right, 4));
    ast_free(p);
}

static void test_all_operators(void)
{
    struct
    {
        const char* src;
        binary_op_t op;
    } cases[] = {
        {"print(1 + 2);", BIN_ADD},
        {"print(1 - 2);", BIN_SUB},
        {"print(1 * 2);", BIN_MUL},
        {"print(1 / 2);", BIN_DIV},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        ast_node_t* p = parse_ok(cases[i].src);
        ast_node_t* e = expr_of(p);
        ASSERT(is_bin(e, cases[i].op));
        ASSERT(is_int(e->binary.left, 1));
        ASSERT(is_int(e->binary.right, 2));
        ast_free(p);
    }
}

static void test_parentheses(void)
{
    /* (1 + 2) * 3 */
    ast_node_t* p = parse_ok("print((1 + 2) * 3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_MUL));
    ASSERT(is_bin(e->binary.left, BIN_ADD));
    ASSERT(is_int(e->binary.right, 3));
    ast_free(p);

    /* Redundant and nested parens. */
    p = parse_ok("print(((((4)))));");
    ASSERT(is_int(expr_of(p), 4));
    ast_free(p);

    /* 1 - (2 - 3) keeps the right-nested shape. */
    p = parse_ok("print(1 - (2 - 3));");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_SUB));
    ASSERT(is_int(e->binary.left, 1));
    ASSERT(is_bin(e->binary.right, BIN_SUB));
    ast_free(p);
}

static void test_binary_spans(void)
{
    const char* src = "print(12 + 345);";
    ast_node_t* p   = parse_ok(src);
    ast_node_t* e   = expr_of(p);

    ASSERT(is_bin(e, BIN_ADD));
    ASSERT(e->span.start == 6 && e->span.end == 14);
    ASSERT(e->binary.left->span.start == 6 && e->binary.left->span.end == 8);
    ASSERT(e->binary.right->span.start == 11 && e->binary.right->span.end == 14);

    ast_free(p);
}

static void test_integer_limits(void)
{
    char src[64];

    snprintf(src, sizeof(src), "print(%ld);", LONG_MAX);
    ast_node_t* p = parse_ok(src);
    ASSERT(is_int(expr_of(p), LONG_MAX));
    ast_free(p);

    p = parse_ok("print(0);");
    ASSERT(is_int(expr_of(p), 0));
    ast_free(p);

    p = parse_ok("print(007);");
    ASSERT(is_int(expr_of(p), 7));
    ast_free(p);
}

static void test_comments_in_program(void)
{
    ast_node_t* p = parse_ok("// header\nlet x = 1; // trailing\nprint(x);\n");
    ASSERT(p && p->program.count == 2);
    ast_free(p);
}

static void test_keywords_not_identifiers(void)
{
    /* `let` / `print` are keywords and can't be used as variable names. */
    expect_fail("let let = 1;");
    expect_fail("let print = 1;");
    expect_fail("print(let);");
}

/* ---- parsing: invalid programs (also exercise error-path frees) -------- */

static void test_syntax_errors(void)
{
    /* Statement level. */
    expect_fail("1;");
    expect_fail("x = 1;");
    expect_fail(";");
    expect_fail("let");
    expect_fail("let x");
    expect_fail("let x =");
    expect_fail("let x = ;");
    expect_fail("let = 1;");
    expect_fail("let 1 = 2;");
    expect_fail("let x = 1");   /* missing semicolon */
    expect_fail("let x 1;");    /* missing '=' */
    expect_fail("print");
    expect_fail("print(");
    expect_fail("print()");
    expect_fail("print();");
    expect_fail("print(1)");    /* missing semicolon */
    expect_fail("print(1;");    /* missing ')' */
    expect_fail("print 1;");    /* missing '(' */

    /* Expression level. */
    expect_fail("print(1 +);");
    expect_fail("print(+ 1);");
    expect_fail("print(1 + * 2);");
    expect_fail("print(1 2);");
    expect_fail("print((1);");
    expect_fail("print(1));");
    expect_fail("print(());");
    expect_fail("print(1 +);");
    expect_fail("print(1 * );");
    expect_fail("print(1 - 2 -);");
    expect_fail("let x = (1 + ;");
    expect_fail("let x = (1 + 2;");

    /* No unary minus in this grammar. */
    expect_fail("print(-1);");
    expect_fail("let x = -1;");

    /* Lexer errors surface as parse errors. */
    expect_fail("let x = 1 @ 2;");
    expect_fail("@");
    expect_fail("print(1) # comment-ish;");
}

static void test_partial_failure_frees_earlier_statements(void)
{
    /* Good statements followed by a bad one: the program built so far must be
     * released without leaks (checked under ASan/valgrind). */
    expect_fail("let a = 1; let b = 2; print(a + b); let = ;");
    expect_fail("let a = (1 + 2) * 3; print(a +");
}

static void test_integer_literal_errors(void)
{
    /* One past LONG_MAX on 64-bit. */
    expect_fail("print(9223372036854775808);");
    expect_fail("print(99999999999999999999999999);");

    /* Longer than the parser's 64-byte buffer. */
    char big[128];
    memset(big, '1', sizeof(big));
    memcpy(big, "print(", 6);
    size_t n = 100;
    big[n]     = ')';
    big[n + 1] = ';';
    big[n + 2] = '\0';
    expect_fail(big);
}

static void test_deep_nesting(void)
{
    /* Moderately deep parens and long chains should work. */
    char   buf[1024];
    size_t len = 0;

    len += (size_t)snprintf(buf + len, sizeof(buf) - len, "print(");
    for (int i = 0; i < 100; i++)
        buf[len++] = '(';
    buf[len++] = '1';
    for (int i = 0; i < 100; i++)
        buf[len++] = ')';
    buf[len++] = ')';
    buf[len++] = ';';
    buf[len]   = '\0';

    ast_node_t* p = parse_ok(buf);
    ASSERT(is_int(expr_of(p), 1));
    ast_free(p);

    len = (size_t)snprintf(buf, sizeof(buf), "print(1");
    for (int i = 0; i < 100; i++)
        len += (size_t)snprintf(buf + len, sizeof(buf) - len, " + 1");
    snprintf(buf + len, sizeof(buf) - len, ");");

    p = parse_ok(buf);
    ast_node_t* e = expr_of(p);
    int depth = 0;
    while (is_bin(e, BIN_ADD))
    {
        depth++;
        e = e->binary.left;
    }
    ASSERT(depth == 100);
    ast_free(p);
}

static void test_parser_respects_source_length(void)
{
    /* Only the first 10 bytes ("let x = 1;") are part of the source. */
    const char text[] = "let x = 1;garbage";
    parser_t   parser;
    parser_init(&parser, (source_t){.data = text, .length = 10});

    ast_node_t* p = parser_parse(&parser);
    ASSERT(p && p->program.count == 1);
    ASSERT(p->span.end == 10);
    ast_free(p);
}

int main(void)
{
    test_ast_constructors();
    test_ast_free_null();
    test_ast_program_push_growth();

    test_empty_program();
    test_let_statement();
    test_let_span_with_leading_text();
    test_statement_spans_in_sequence();
    test_print_statement();
    test_multiple_statements();
    test_many_statements();
    test_identifier_expression();
    test_precedence();
    test_left_associativity();
    test_all_operators();
    test_parentheses();
    test_binary_spans();
    test_integer_limits();
    test_comments_in_program();
    test_keywords_not_identifiers();

    test_syntax_errors();
    test_partial_failure_frees_earlier_statements();
    test_integer_literal_errors();
    test_deep_nesting();
    test_parser_respects_source_length();

    TEST_EXIT();
}
