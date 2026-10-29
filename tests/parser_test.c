#include "ast.h"
#include "parser.h"
#include "test.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static ast_node_t* parse(const char* text)
{
    parser_t parser;
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});
    return parser_parse(&parser);
}

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
    parser_t parser;
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});

    ast_node_t* program = parser_parse(&parser);
    if (program)
        fprintf(stderr, "  \"%s\" should not have parsed\n", text);
    ASSERT(program == NULL);
    ast_free(program);

    const diagnostic_t* d = parser_error(&parser);
    if (!d)
        fprintf(stderr, "  \"%s\" failed without a diagnostic\n", text);
    ASSERT(d != NULL);

    if (d)
    {
        ASSERT(d->message[0] != '\0');
        ASSERT(strchr(d->message, '\n') == NULL);
        ASSERT(d->span.start <= d->span.end);
        ASSERT(d->span.end <= strlen(text));
    }
}

static void expect_diag(const char* text,
                        const char* message,
                        size_t      start,
                        size_t      end)
{
    parser_t parser;
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});

    ast_node_t* program = parser_parse(&parser);
    ASSERT(program == NULL);
    ast_free(program);

    const diagnostic_t* d = parser_error(&parser);
    ASSERT(d != NULL);
    if (!d)
        return;

    if (strcmp(d->message, message) != 0 || d->span.start != start ||
        d->span.end != end)
    {
        fprintf(stderr,
                "  in \"%s\":\n    got  [%zu,%zu) \"%s\"\n    want [%zu,%zu) "
                "\"%s\"\n",
                text,
                d->span.start,
                d->span.end,
                d->message,
                start,
                end,
                message);
    }

    ASSERT(strcmp(d->message, message) == 0);
    ASSERT(d->span.start == start);
    ASSERT(d->span.end == end);
}

static ast_node_t* expr_of(ast_node_t* program)
{
    ASSERT(program && program->kind == AST_PROGRAM);
    ASSERT(program->program.count == 1);
    ast_node_t* stmt = program->program.statements[0];
    ASSERT(stmt->kind == AST_PRINT);
    return stmt->print.value;
}

static bool is_int(const ast_node_t* n, int64_t v)
{
    return n && n->kind == AST_INTEGER && n->integer == v;
}

static bool is_bin(const ast_node_t* n, binary_op_t op)
{
    return n && n->kind == AST_BINARY && n->binary.op == op;
}

static bool is_neg(const ast_node_t* n)
{
    return n && n->kind == AST_UNARY && n->unary.op == UN_NEG;
}

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

    ast_node_t* u = ast_unary(sp, UN_NEG, ast_integer(sp, 4));
    ASSERT(u && u->kind == AST_UNARY && u->unary.op == UN_NEG);
    ASSERT(is_int(u->unary.operand, 4));
    ASSERT(u->span.start == 3 && u->span.end == 5);
    ast_free(u);

    ast_free(l);
    ast_free(p);
}

static void test_ast_free_null(void)
{
    ast_free(NULL);
    ASSERT(1);
}

static void test_ast_program_push_growth(void)
{
    ast_node_t* program = ast_program();
    ASSERT(program && program->kind == AST_PROGRAM);
    ASSERT(program->program.count == 0);
    ASSERT(program->program.statements == NULL);

    for (int64_t i = 0; i < 50; i++)
    {
        ast_node_t* n = ast_integer((source_span_t){0, 0}, i);
        ASSERT(n != NULL);
        ASSERT(ast_program_push(program, n));
    }

    ASSERT(program->program.count == 50);
    ASSERT(program->program.capacity >= 50);

    for (int64_t i = 0; i < 50; i++)
        ASSERT(is_int(program->program.statements[i], i));

    ast_free(program);
}

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
    ASSERT(s->span.start == 0 && s->span.end == strlen(src));

    ast_free(p);
}

static void test_let_span_with_leading_text(void)
{
    const char* src = "  // c\n  let   foo = 1 + 2 ;";
    ast_node_t* p   = parse_ok(src);

    ASSERT(p && p->program.count == 1);
    ast_node_t* s = p->program.statements[0];
    ASSERT(s->kind == AST_LET);
    ASSERT(s->span.start == 9);
    ASSERT(s->span.end == strlen(src));
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
    char   buf[2048];
    size_t len = 0;
    for (int i = 0; i < 40; i++)
        len += (size_t)snprintf(buf + len, sizeof(buf) - len, "print(%d);", i);

    ast_node_t* p = parse_ok(buf);
    ASSERT(p && p->program.count == 40);
    for (int64_t i = 0; i < 40; i++)
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
    ast_node_t* p = parse_ok("print(1 + 2 * 3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_ADD));
    ASSERT(is_int(e->binary.left, 1));
    ASSERT(is_bin(e->binary.right, BIN_MUL));
    ASSERT(is_int(e->binary.right->binary.left, 2));
    ASSERT(is_int(e->binary.right->binary.right, 3));
    ast_free(p);

    p = parse_ok("print(1 * 2 + 3);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_ADD));
    ASSERT(is_bin(e->binary.left, BIN_MUL));
    ASSERT(is_int(e->binary.right, 3));
    ast_free(p);
}

static void test_left_associativity(void)
{
    ast_node_t* p = parse_ok("print(10 - 3 - 2);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_SUB));
    ASSERT(is_int(e->binary.right, 2));
    ASSERT(is_bin(e->binary.left, BIN_SUB));
    ASSERT(is_int(e->binary.left->binary.left, 10));
    ASSERT(is_int(e->binary.left->binary.right, 3));
    ast_free(p);

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
    ast_node_t* p = parse_ok("print((1 + 2) * 3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_MUL));
    ASSERT(is_bin(e->binary.left, BIN_ADD));
    ASSERT(is_int(e->binary.right, 3));
    ast_free(p);

    p = parse_ok("print(((((4)))));");
    ASSERT(is_int(expr_of(p), 4));
    ast_free(p);

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
    ASSERT(e->binary.right->span.start == 11 &&
           e->binary.right->span.end == 14);

    ast_free(p);
}

static void test_unary_minus(void)
{
    const char* src = "print(-1);";
    ast_node_t* p   = parse_ok(src);
    ast_node_t* e   = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(is_int(e->unary.operand, 1));
    ASSERT(e->span.start == 6 && e->span.end == 8);
    ast_free(p);

    p = parse_ok("let x = -5;");
    ASSERT(is_neg(p->program.statements[0]->let.value));
    ast_free(p);

    p = parse_ok("print(-foo);");
    e = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(e->unary.operand->kind == AST_IDENTIFIER);
    ast_free(p);

    p = parse_ok("print(-(1 + 2));");
    e = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(is_bin(e->unary.operand, BIN_ADD));
    ast_free(p);
}

static void test_unary_precedence(void)
{
    ast_node_t* p = parse_ok("print(-2 * 3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_bin(e, BIN_MUL));
    ASSERT(is_neg(e->binary.left));
    ASSERT(is_int(e->binary.left->unary.operand, 2));
    ASSERT(is_int(e->binary.right, 3));
    ast_free(p);

    p = parse_ok("print(2 - -3);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_SUB));
    ASSERT(is_int(e->binary.left, 2));
    ASSERT(is_neg(e->binary.right));
    ASSERT(is_int(e->binary.right->unary.operand, 3));
    ast_free(p);

    p = parse_ok("print(2 * -3);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_MUL));
    ASSERT(is_neg(e->binary.right));
    ast_free(p);
}

static void test_unary_nested(void)
{
    ast_node_t* p = parse_ok("print(- -3);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(is_neg(e->unary.operand));
    ASSERT(is_int(e->unary.operand->unary.operand, 3));
    ASSERT(e->span.start == 6 && e->span.end == 10);
    ast_free(p);

    p = parse_ok("print(--3);");
    e = expr_of(p);
    ASSERT(is_neg(e) && is_neg(e->unary.operand));
    ast_free(p);
}

static void test_integer_limits(void)
{
    char src[64];

    snprintf(src, sizeof(src), "print(%" PRId64 ");", (int64_t)INT64_MAX);
    ast_node_t* p = parse_ok(src);
    ASSERT(is_int(expr_of(p), (int64_t)INT64_MAX));
    ast_free(p);

    p = parse_ok("print(0);");
    ASSERT(is_int(expr_of(p), 0));
    ast_free(p);

    p = parse_ok("print(007);");
    ASSERT(is_int(expr_of(p), 7));
    ast_free(p);
}

static void test_int64_min_literal(void)
{
    const char* src = "print(-9223372036854775808);";
    ast_node_t* p   = parse_ok(src);
    ast_node_t* e   = expr_of(p);

    ASSERT(is_int(e, INT64_MIN));
    ASSERT(e->span.start == 6);
    ASSERT(e->span.end == 26);
    ast_free(p);

    p = parse_ok("let x = -9223372036854775808;");
    ASSERT(is_int(p->program.statements[0]->let.value, INT64_MIN));
    ast_free(p);

    p = parse_ok("print(1 - -9223372036854775808);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_SUB));
    ASSERT(is_int(e->binary.right, INT64_MIN));
    ast_free(p);

    p = parse_ok("print(-9223372036854775808 * 2);");
    e = expr_of(p);
    ASSERT(is_bin(e, BIN_MUL));
    ASSERT(is_int(e->binary.left, INT64_MIN));
    ast_free(p);
}

static void test_negated_literals_stay_unary(void)
{
    ast_node_t* p = parse_ok("print(-9223372036854775807);");
    ast_node_t* e = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(is_int(e->unary.operand, INT64_MAX));
    ast_free(p);

    p = parse_ok("print(- -9223372036854775808);");
    e = expr_of(p);
    ASSERT(is_neg(e));
    ASSERT(is_int(e->unary.operand, INT64_MIN));
    ast_free(p);
}

static void test_leading_zeros(void)
{
    enum
    {
        PREFIX = 6,
        ZEROS  = 70,
    };

    char src[PREFIX + ZEROS + 32];

    memcpy(src, "print(", PREFIX);
    memset(src + PREFIX, '0', ZEROS);

    strcpy(src + PREFIX + ZEROS, "1);");
    ast_node_t* p = parse_ok(src);
    ASSERT(is_int(expr_of(p), 1));
    ast_free(p);

    strcpy(src + PREFIX + ZEROS, "9223372036854775808);");
    expect_fail(src);

    strcpy(src + PREFIX + ZEROS, "9223372036854775807);");
    p = parse_ok(src);
    ASSERT(is_int(expr_of(p), INT64_MAX));
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
    expect_fail("let let = 1;");
    expect_fail("let print = 1;");
    expect_fail("print(let);");
}

static void test_syntax_errors(void)
{
    expect_fail("1;");
    expect_fail("x = 1;");
    expect_fail(";");
    expect_fail("let");
    expect_fail("let x");
    expect_fail("let x =");
    expect_fail("let x = ;");
    expect_fail("let = 1;");
    expect_fail("let 1 = 2;");
    expect_fail("let x = 1");
    expect_fail("let x 1;");
    expect_fail("print");
    expect_fail("print(");
    expect_fail("print()");
    expect_fail("print();");
    expect_fail("print(1)");
    expect_fail("print(1;");
    expect_fail("print 1;");

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

    expect_fail("print(-);");
    expect_fail("let x = -;");
    expect_fail("let x = - * 2;");
    expect_fail("print(1 - -);");
    expect_fail("print(-");

    expect_fail("let x = 1 @ 2;");
    expect_fail("@");
    expect_fail("print(1) # comment-ish;");
}

static void test_partial_failure_frees_earlier_statements(void)
{
    expect_fail("let a = 1; let b = 2; print(a + b); let = ;");
    expect_fail("let a = (1 + 2) * 3; print(a +");
}

static void test_integer_literal_errors(void)
{
    expect_fail("print(9223372036854775808);");
    expect_fail("print(99999999999999999999999999);");
    expect_fail("print(-9223372036854775809);");
    expect_fail("print(-(9223372036854775808));");
    expect_fail("print(1 -9223372036854775808);");
    expect_fail("print(18446744073709551615);");
    expect_fail("print(18446744073709551616);");

    char big[128];
    memset(big, '1', sizeof(big));
    memcpy(big, "print(", 6);
    size_t n   = 100;
    big[n]     = ')';
    big[n + 1] = ';';
    big[n + 2] = '\0';
    expect_fail(big);
}

static void test_deep_nesting(void)
{
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

    p                 = parse_ok(buf);
    ast_node_t* e     = expr_of(p);
    int         depth = 0;
    while (is_bin(e, BIN_ADD))
    {
        depth++;
        e = e->binary.left;
    }
    ASSERT(depth == 100);
    ast_free(p);
}

static void test_valid_parse_has_no_error(void)
{
    parser_t    parser;
    const char* text = "let x = 1;";
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});

    ast_node_t* p = parser_parse(&parser);
    ASSERT(p != NULL);
    ASSERT(parser_error(&parser) == NULL);
    ast_free(p);
}

static void test_diagnostic_messages(void)
{
    expect_diag("x = 1;",
                "expected a statement ('let' or 'print'), found identifier 'x'",
                0,
                1);
    expect_diag("1;",
                "expected a statement ('let' or 'print'), found number '1'",
                0,
                1);
    expect_diag("let = 1;",
                "expected a variable name after 'let', found '='",
                4,
                5);
    expect_diag("let let = 1;",
                "expected a variable name after 'let', found 'let'",
                4,
                7);
    expect_diag("let x 1;",
                "expected '=' after the variable name, found number '1'",
                6,
                7);
    expect_diag("let x = ;", "expected an expression, found ';'", 8, 9);
    expect_diag("print 1;",
                "expected '(' after 'print', found number '1'",
                6,
                7);

    expect_diag("let x = 1",
                "expected ';' after the let statement, found end of input",
                9,
                9);
    expect_diag("let x = 1;\nlet y = 2\n\n",
                "expected ';' after the let statement, found end of input",
                20,
                20);
    expect_diag("print(1",
                "expected ')' after the expression, found end of input",
                7,
                7);
    expect_diag("let x =\n",
                "expected an expression, found end of input",
                7,
                7);
    expect_diag("let",
                "expected a variable name after 'let', found end of input",
                3,
                3);

    expect_diag("let x = 1 print(x);",
                "expected ';' after the let statement, found 'print'",
                10,
                15);
    expect_diag("print(1) 2;",
                "expected ';' after the print statement, found number '2'",
                9,
                10);

    expect_diag("print((1);",
                "expected ')' after the expression, found ';'",
                9,
                10);
    expect_diag("print(1 + (2;",
                "expected ')' to close the '(', found ';'",
                12,
                13);
    expect_diag("print(());", "expected an expression, found ')'", 7, 8);

    expect_diag("print(1 + * 2);", "expected an expression, found '*'", 10, 11);
    expect_diag("print(-);", "expected an expression, found ')'", 7, 8);
    expect_diag("print(1 2);",
                "expected ')' after the expression, found number '2'",
                8,
                9);

    expect_diag("let a = 1;\nlet b = ;",
                "expected an expression, found ';'",
                19,
                20);
}

static void test_invalid_character_diagnostics(void)
{
    expect_diag("let x = 1 @ 2;", "invalid character '@'", 10, 11);
    expect_diag("@", "invalid character '@'", 0, 1);
    expect_diag("let # = 1;", "invalid character '#'", 4, 5);
    expect_diag("print(1 $);", "invalid character '$'", 8, 9);
    expect_diag("print(\x01);", "invalid byte 0x01", 6, 7);
}

static void test_integer_diagnostics(void)
{
    expect_diag("print(9223372036854775808);",
                "integer literal is too large (maximum is 9223372036854775807)",
                6,
                25);

    char big[160];
    memcpy(big, "print(", 6);
    memset(big + 6, '9', 80);
    memcpy(big + 86, ");", 3);
    expect_diag(big,
                "integer literal is too large (maximum is 9223372036854775807)",
                6,
                86);

    expect_diag("print(-9223372036854775809);",
                "integer literal is too large (maximum is 9223372036854775807)",
                7,
                26);
}

static void test_long_names_are_clipped_in_messages(void)
{
    char text[64];
    memset(text, 'a', 40);
    text[40] = ';';
    text[41] = '\0';

    parser_t parser;
    parser_init(&parser, (source_t){.data = text, .length = strlen(text)});
    ASSERT(parser_parse(&parser) == NULL);

    const diagnostic_t* d = parser_error(&parser);
    ASSERT(d != NULL);
    if (d)
    {
        ASSERT(strstr(d->message, "identifier 'aaaaaaaaaaaaaaaaaaaaaaaa...'") !=
               NULL);
        ASSERT(d->span.start == 0 && d->span.end == 40);
    }
}

static void test_first_error_wins(void)
{
    expect_diag("let = 1; let = 2;",
                "expected a variable name after 'let', found '='",
                4,
                5);
}

static void test_error_state_resets_on_init(void)
{
    parser_t    parser;
    const char* bad  = "let;";
    const char* good = "let x = 1;";

    parser_init(&parser, (source_t){.data = bad, .length = strlen(bad)});
    ASSERT(parser_parse(&parser) == NULL);
    ASSERT(parser_error(&parser) != NULL);

    parser_init(&parser, (source_t){.data = good, .length = strlen(good)});
    ast_node_t* p = parser_parse(&parser);
    ASSERT(p != NULL);
    ASSERT(parser_error(&parser) == NULL);
    ast_free(p);
}

static void test_parser_respects_source_length(void)
{
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
    test_unary_minus();
    test_unary_precedence();
    test_unary_nested();
    test_integer_limits();
    test_int64_min_literal();
    test_negated_literals_stay_unary();
    test_leading_zeros();
    test_comments_in_program();
    test_keywords_not_identifiers();

    test_valid_parse_has_no_error();
    test_diagnostic_messages();
    test_invalid_character_diagnostics();
    test_integer_diagnostics();
    test_long_names_are_clipped_in_messages();
    test_first_error_wins();
    test_error_state_resets_on_init();

    test_syntax_errors();
    test_partial_failure_frees_earlier_statements();
    test_integer_literal_errors();
    test_deep_nesting();
    test_parser_respects_source_length();

    TEST_EXIT();
}
