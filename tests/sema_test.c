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

static void expect_sema_error(const char* text,
                              const char* message,
                              size_t      start,
                              size_t      end)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, text, &ok);

    ASSERT(!ok);

    const diagnostic_t* d = sema_error(&sema);
    ASSERT(d != nullptr);
    if (d && (strcmp(d->message, message) != 0 || d->span.start != start ||
              d->span.end != end))
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
    ASSERT(d && strcmp(d->message, message) == 0);
    ASSERT(d && d->span.start == start && d->span.end == end);

    ast_free(p);
    sema_free(&sema);
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

static void test_block_variable_is_local(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{ let a = 1; } print(a);", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);
    ASSERT(sema_error(&sema)->span.start == 21);

    ast_free(p);
    sema_free(&sema);
}

static void test_nested_block_variable_is_local(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{ { let a = 1; } print(a); }", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);

    ast_free(p);
    sema_free(&sema);
}

static void test_block_sees_outer_variable(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; { { print(a); } }", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 1);

    ast_node_t* inner = p->program.statements[1]->block.statements[0];
    ASSERT(inner->block.statements[0]->print.value->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_block_assigns_outer_variable(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; { a = 2; } print(a);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 1);

    ast_node_t* assign = p->program.statements[1]->block.statements[0];
    ASSERT(assign->kind == AST_ASSIGN && assign->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_block_shadows_outer_variable(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema, "let a = 1; { let a = 2; print(a); } print(a);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 2);

    ast_node_t* block = p->program.statements[1];
    ASSERT(block->block.statements[0]->slot == 1);
    ASSERT(block->block.statements[1]->print.value->slot == 1);
    ASSERT(p->program.statements[2]->print.value->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_shadow_initializer_sees_outer_variable(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema, "let a = 1; { let a = a + 1; print(a); }", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 2);

    ast_node_t* let = p->program.statements[1]->block.statements[0];
    ASSERT(let->slot == 1);
    ASSERT(let->let.value->binary.left->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_targets_innermost_shadow(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema, "let a = 1; { let a = 2; a = 3; } a = 4;", &ok);

    ASSERT(ok);

    ast_node_t* block = p->program.statements[1];
    ASSERT(block->block.statements[1]->slot == 1);
    ASSERT(p->program.statements[2]->slot == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_block_redeclaration(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{ let a = 1; let a = 2; }", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "already declared") != nullptr);
    ASSERT(sema_error(&sema)->span.start == 17);

    ast_free(p);
    sema_free(&sema);
}

static void test_redeclaration_after_inner_scope(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; { let b = 2; } let a = 3;", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "already declared") != nullptr);

    ast_free(p);
    sema_free(&sema);
}

static void test_names_are_free_after_block(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema, "{ let a = 1; } { let a = 2; } let a = 3; print(a);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 3);
    ASSERT(p->program.statements[3]->print.value->slot == 2);

    ast_free(p);
    sema_free(&sema);
}

static void test_assign_before_declare_in_block(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{ a = 1; let a = 2; }", &ok);

    ASSERT(!ok);
    ASSERT(strstr(sema_error(&sema)->message, "undeclared") != nullptr);

    ast_free(p);
    sema_free(&sema);
}

static void test_empty_block_declares_nothing(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{} { {} } print(1);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_many_blocks_do_not_accumulate_symbols(void)
{
    char   text[4096];
    size_t length = 0;

    for (int i = 0; i < 50; i++)
    {
        length += (size_t)snprintf(text + length, sizeof(text) - length, "{ ");

        for (int j = 0; j < 4; j++)
            length += (size_t)snprintf(text + length,
                                       sizeof(text) - length,
                                       "let v%d = %d; ",
                                       j,
                                       j);

        length += (size_t)snprintf(text + length, sizeof(text) - length, "} ");
    }

    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, text, &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 200);
    ASSERT(sema.symbol_count == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_expression_types(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; print(-a + 2 * a);", &ok);

    ASSERT(ok);
    ASSERT(p->program.statements[0]->let.value->type == TYPE_INT);

    ast_node_t* sum = p->program.statements[1]->print.value;
    ASSERT(sum->kind == AST_BINARY && sum->type == TYPE_INT);

    ast_node_t* neg = sum->binary.left;
    ASSERT(neg->kind == AST_UNARY && neg->type == TYPE_INT);
    ASSERT(neg->unary.operand->kind == AST_IDENTIFIER);
    ASSERT(neg->unary.operand->type == TYPE_INT);

    ast_node_t* product = sum->binary.right;
    ASSERT(product->kind == AST_BINARY && product->type == TYPE_INT);
    ASSERT(product->binary.left->type == TYPE_INT);
    ASSERT(product->binary.right->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_statements_have_no_type(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; a = 2; print(a); { }", &ok);

    ASSERT(ok && p->program.count == 4);
    ASSERT(p->program.statements[0]->type == TYPE_NONE);
    ASSERT(p->program.statements[1]->type == TYPE_NONE);
    ASSERT(p->program.statements[2]->type == TYPE_NONE);
    ASSERT(p->program.statements[3]->type == TYPE_NONE);

    ast_free(p);
    sema_free(&sema);
}

static void test_symbols_record_types(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let a = 1; let b = a + 1;", &ok);

    ASSERT(ok);
    ASSERT(sema.symbol_count == 2);
    ASSERT(sema.symbols[0].type == TYPE_INT);
    ASSERT(sema.symbols[1].type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_identifier_takes_declared_type(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema,
              "let a = 1; { let a = 2; print(a); } a = 3; print(a);",
              &ok);

    ASSERT(ok);

    ast_node_t* block = p->program.statements[1];
    ASSERT(block->block.statements[1]->print.value->type == TYPE_INT);
    ASSERT(p->program.statements[3]->print.value->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_types_are_set_inside_nested_blocks(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "{ let b = 1; { print(b + 1); } }", &ok);

    ASSERT(ok);

    ast_node_t* outer = p->program.statements[0];
    ast_node_t* inner = outer->block.statements[1];
    ast_node_t* sum   = inner->block.statements[0]->print.value;
    ASSERT(sum->type == TYPE_INT);
    ASSERT(sum->binary.left->type == TYPE_INT);
    ASSERT(sum->binary.right->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_boolean_literal_types(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(true); print(false); print(1);", &ok);

    ASSERT(ok);
    ASSERT(p->program.statements[0]->print.value->type == TYPE_BOOL);
    ASSERT(p->program.statements[1]->print.value->type == TYPE_BOOL);
    ASSERT(p->program.statements[2]->print.value->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_boolean_variables_keep_their_type(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema, "let b = true; let n = 1; print(b); print(n);", &ok);

    ASSERT(ok);
    ASSERT(sema.symbols[0].type == TYPE_BOOL);
    ASSERT(sema.symbols[1].type == TYPE_INT);
    ASSERT(p->program.statements[2]->print.value->type == TYPE_BOOL);
    ASSERT(p->program.statements[3]->print.value->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_boolean_assignment(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "let b = true; b = false; b = b;", &ok);

    ASSERT(ok);
    ASSERT(p->program.statements[1]->assign.value->type == TYPE_BOOL);

    ast_free(p);
    sema_free(&sema);
}

static void test_assignment_cannot_change_type(void)
{
    expect_sema_error("let b = true; b = 1;",
                      "expected bool, found int",
                      18,
                      19);
    expect_sema_error("let a = 1; a = true;",
                      "expected int, found bool",
                      15,
                      19);
    expect_sema_error("let a = 1; let b = true; a = b;",
                      "expected int, found bool",
                      29,
                      30);
}

static void test_arithmetic_requires_ints(void)
{
    expect_sema_error("print(true + 1);", "expected int, found bool", 6, 10);
    expect_sema_error("print(1 + false);", "expected int, found bool", 10, 15);
    expect_sema_error("print(true * true);", "expected int, found bool", 6, 10);
    expect_sema_error("print(1 - (2 / true));",
                      "expected int, found bool",
                      15,
                      19);
    expect_sema_error("let b = true; print(b - 1);",
                      "expected int, found bool",
                      20,
                      21);
}

static void test_negation_requires_an_int(void)
{
    expect_sema_error("print(-true);", "expected int, found bool", 7, 11);
    expect_sema_error("let b = true; print(-b);",
                      "expected int, found bool",
                      21,
                      22);
}

static void test_print_accepts_both_types(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p = check(&sema, "print(1 + 2); print(true);", &ok);

    ASSERT(ok);
    ASSERT(sema.slot_count == 0);

    ast_free(p);
    sema_free(&sema);
}

static void test_shadowing_may_change_type(void)
{
    sema_t sema;
    bool   ok;

    ast_node_t* p =
        check(&sema,
              "let a = 1; { let a = true; print(a); } a = 2; print(a);",
              &ok);

    ASSERT(ok);

    ast_node_t* block = p->program.statements[1];
    ASSERT(block->block.statements[1]->print.value->type == TYPE_BOOL);
    ASSERT(p->program.statements[3]->print.value->type == TYPE_INT);

    ast_free(p);
    sema_free(&sema);
}

static void test_shadowed_variable_keeps_inner_type(void)
{
    expect_sema_error("let a = 1; { let a = true; a = 2; }",
                      "expected bool, found int",
                      31,
                      32);
}

static void test_types_are_checked_inside_blocks(void)
{
    expect_sema_error("{ let b = true; { print(b * 2); } }",
                      "expected int, found bool",
                      24,
                      25);
}

static void test_first_type_error_wins(void)
{
    expect_sema_error("print(true + false); print(-true);",
                      "expected int, found bool",
                      6,
                      10);
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

    test_block_variable_is_local();
    test_nested_block_variable_is_local();
    test_block_sees_outer_variable();
    test_block_assigns_outer_variable();
    test_block_shadows_outer_variable();
    test_shadow_initializer_sees_outer_variable();
    test_assign_targets_innermost_shadow();
    test_block_redeclaration();
    test_redeclaration_after_inner_scope();
    test_names_are_free_after_block();
    test_assign_before_declare_in_block();
    test_empty_block_declares_nothing();
    test_many_blocks_do_not_accumulate_symbols();

    test_expression_types();
    test_statements_have_no_type();
    test_symbols_record_types();
    test_identifier_takes_declared_type();
    test_types_are_set_inside_nested_blocks();

    test_boolean_literal_types();
    test_boolean_variables_keep_their_type();
    test_boolean_assignment();
    test_assignment_cannot_change_type();
    test_arithmetic_requires_ints();
    test_negation_requires_an_int();
    test_print_accepts_both_types();
    test_shadowing_may_change_type();
    test_shadowed_variable_keeps_inner_type();
    test_types_are_checked_inside_blocks();
    test_first_type_error_wins();

    TEST_EXIT();
}
