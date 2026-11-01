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

static void test_assign_resolves_slots(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; let b = 2; b = a; print(b);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 2);

    ast_node_t* assign = p->program.statements[2];
    ASSERT(assign->kind == AST_ASSIGN);
    ASSERT(assign->slot == 1);
    ASSERT(assign->assign.value->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_does_not_allocate_slots(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let x = 1; x = 2; x = 3;", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 1);
    ASSERT(p->program.statements[1]->slot == 0);
    ASSERT(p->program.statements[2]->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_reads_old_value(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let x = 1; x = x + 1;", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 1);

    ast_node_t* value = p->program.statements[1]->assign.value;
    ASSERT(value->kind == AST_BINARY);
    ASSERT(value->binary.left->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_undeclared_target(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; b = a;", &ok);

    ASSERT(!ok);
    ASSERT(sema_error(&sema) != nullptr);
    ASSERT(strcmp(sema_error(&sema)->message,
                  "assignment to undeclared variable 'b'") == 0);
    ASSERT(sema_error(&sema)->span.start == 11);
    ASSERT(sema_error(&sema)->span.end == 12);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_before_declare(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "x = 1; let x = 2;", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "assignment to undeclared") !=
           nullptr);
    ASSERT(sema_error(&sema)->span.start == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_undeclared_value(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let x = 1; x = y;", &ok);

    ASSERT(!ok);
    ASSERT(strcmp(sema_error(&sema)->message,
                  "use of undeclared variable 'y'") == 0);
    ASSERT(sema_error(&sema)->span.start == 15);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_checks_value_before_target(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "x = y;", &ok);

    ASSERT(!ok);
    ASSERT(strcmp(sema_error(&sema)->message,
                  "use of undeclared variable 'y'") == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_after_failure_keeps_first_error(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(a); b = 1;", &ok);

    ASSERT(!ok);
    ASSERT(strcmp(sema_error(&sema)->message,
                  "use of undeclared variable 'a'") == 0);

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

    test_assign_resolves_slots();
    test_assign_does_not_allocate_slots();
    test_assign_reads_old_value();
    test_assign_undeclared_target();
    test_assign_before_declare();
    test_assign_undeclared_value();
    test_assign_checks_value_before_target();
    test_assign_after_failure_keeps_first_error();

    TEST_EXIT();
}
