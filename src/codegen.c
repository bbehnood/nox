#include "codegen.h"

#include "ast.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define SLOT_SIZE (8)

#define DIV_ZERO_MESSAGE "nox: runtime error: division by zero\\n"

typedef struct codegen
{
    FILE*    out;
    unsigned labels;
} codegen_t;

static size_t slot_offset(size_t slot)
{
    return (slot + 1) * SLOT_SIZE;
}

static void gen_node(codegen_t* gen, const ast_node_t* node);

static void gen_division(codegen_t* gen)
{
    unsigned label = gen->labels++;

    fprintf(gen->out,
            "    test rcx, rcx\n"
            "    jz .Ldiv_zero\n"
            "    cmp rcx, -1\n"
            "    je .Ldiv_neg_%u\n"
            "    cqo\n"
            "    idiv rcx\n"
            "    jmp .Ldiv_end_%u\n"
            ".Ldiv_neg_%u:\n"
            "    neg rax\n"
            ".Ldiv_end_%u:\n",
            label,
            label,
            label,
            label);
}

static void gen_binary(codegen_t* gen, const ast_node_t* node)
{
    gen_node(gen, node->binary.left);
    fprintf(gen->out, "    push rax\n");

    gen_node(gen, node->binary.right);
    fprintf(gen->out,
            "    mov rcx, rax\n"
            "    pop rax\n");

    switch (node->binary.op)
    {
    case BIN_ADD:
        fprintf(gen->out, "    add rax, rcx\n");
        break;

    case BIN_SUB:
        fprintf(gen->out, "    sub rax, rcx\n");
        break;

    case BIN_MUL:
        fprintf(gen->out, "    imul rax, rcx\n");
        break;

    case BIN_DIV:
        gen_division(gen);
        break;
    }
}

static void gen_node(codegen_t* gen, const ast_node_t* node)
{
    switch (node->kind)
    {
    case AST_INTEGER:
        fprintf(gen->out, "    mov rax, %" PRId64 "\n", node->integer);
        break;

    case AST_IDENTIFIER:
        fprintf(gen->out,
                "    mov rax, [rbp - %zu]\n",
                slot_offset(node->slot));
        break;

    case AST_BINARY:
        gen_binary(gen, node);
        break;

    case AST_UNARY:
        gen_node(gen, node->unary.operand);

        switch (node->unary.op)
        {
        case UN_NEG:
            fprintf(gen->out, "    neg rax\n");
            break;
        }
        break;

    case AST_LET:
        gen_node(gen, node->let.value);
        fprintf(gen->out,
                "    mov [rbp - %zu], rax\n",
                slot_offset(node->slot));
        break;

    case AST_PRINT:
        gen_node(gen, node->print.value);
        fprintf(gen->out,
                "    mov rsi, rax\n"
                "    lea rdi, [rip + .Lfmt]\n"
                "    xor eax, eax\n"
                "    call printf@PLT\n");
        break;

    case AST_PROGRAM:
        for (size_t i = 0; i < node->program.count; i++)
            gen_node(gen, node->program.statements[i]);
        break;
    }
}

bool codegen_emit(FILE* out, const ast_node_t* program, size_t slot_count)
{
#if !defined(__x86_64__) || !defined(__linux__)
#error "nox currently targets x86-64 Linux only"
#endif
    codegen_t gen = {.out = out, .labels = 0};

    size_t frame = (slot_count * SLOT_SIZE + 15) & ~(size_t)15;

    fprintf(out,
            "    .intel_syntax noprefix\n"
            "    .section .rodata\n"
            ".Lfmt:\n"
            "    .asciz \"%%ld\\n\"\n"
            ".Ldiv_zero_msg:\n"
            "    .ascii \"" DIV_ZERO_MESSAGE "\"\n"
            "    .set .Ldiv_zero_len, . - .Ldiv_zero_msg\n"
            "\n"
            "    .text\n"
            "    .globl main\n"
            "    .type main, @function\n"
            "main:\n"
            "    push rbp\n"
            "    mov rbp, rsp\n");

    if (frame > 0)
        fprintf(out, "    sub rsp, %zu\n", frame);

    gen_node(&gen, program);

    fprintf(out,
            "    xor eax, eax\n"
            "    leave\n"
            "    ret\n"
            "    .size main, . - main\n"
            "\n"
            ".Ldiv_zero:\n"
            "    and rsp, -16\n"
            "    mov edi, 2\n"
            "    lea rsi, [rip + .Ldiv_zero_msg]\n"
            "    mov edx, .Ldiv_zero_len\n"
            "    call write@PLT\n"
            "    mov edi, 1\n"
            "    call exit@PLT\n"
            "\n"
            "    .section .note.GNU-stack,\"\",@progbits\n");

    return fflush(out) == 0 && ferror(out) == 0;
}
