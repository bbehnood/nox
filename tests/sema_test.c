#include "ast.h"
#include "parser.h"
#include "sema.h"
#include "test.h"

#include <string.h>

static ast_node_t* check(sema_t* sema, const char* text, bool* ok)
{
    parser_t parser;
    parser_init(&parser, SRC(text));

    ast_node_t* program = parser_parse(&parser);
    ASSERT(program != nullptr);

    sema_init(sema, SRC(text));
    *ok = program && sema_check(sema, program);
    return program;
}

static void test_assigns_slots(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; let b = a + 2; print(b);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 2);
    ASSERT(p->program.statements[0]->slot == 0);
    ASSERT(p->program.statements[1]->slot == 1);

    ASSERT(p->program.statements[1]->let.value->binary.left->slot == 0);
    ASSERT(p->program.statements[2]->print.value->slot == 1);

    ast_free(p);
    sema_free(&sema);
}

static void test_no_variables(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(1 + 2);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 0);
    ASSERT(sema_error(&sema) == nullptr);

    ast_free(p);
    sema_free(&sema);
}

static void test_undeclared(void)
{
    sema_t sema;
    bool   ok;

    const char* text = "print(x);";
    ast_node_t* p    = check(&sema, text, &ok);

    ASSERT(!ok);
    ASSERT(sema_error(&sema) != nullptr);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);
    ASSERT(sema_error(&sema)->span.start == 6);

    ast_free(p);
    sema_free(&sema);
}

static void test_use_before_declare(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(x); let x = 1;", &ok);

    ASSERT(!ok);

    ast_free(p);
    sema_free(&sema);
}

static void test_self_reference(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let x = x;", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);

    ast_free(p);
    sema_free(&sema);
}

static void test_redeclaration(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let x = 1; let x = 2;", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "already declared") != nullptr);
    ASSERT(sema_error(&sema)->span.start == 15);

    ast_free(p);
    sema_free(&sema);
}

static void test_prefix_names_differ(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; let ab = 2; print(a + ab);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 2);

    ast_free(p);
    sema_free(&sema);
}

static void test_unary_resolves_operand(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; print(-a);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 1);

    ast_free(p);
    sema_free(&sema);
}

static void test_unary_undeclared(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(-b);", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);

    ast_free(p);
    sema_free(&sema);
}

int main(void)
{
    test_assigns_slots();
    test_no_variables();
    test_undeclared();
    test_use_before_declare();
    test_self_reference();
    test_redeclaration();
    test_prefix_names_differ();
    test_unary_resolves_operand();
    test_unary_undeclared();

    TEST_EXIT();
}
