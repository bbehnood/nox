#include "codegen.h"

#include "ast.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static constexpr size_t SLOT_SIZE       = 8;
static constexpr size_t STACK_ALIGNMENT = 16;

#define DIV_ZERO_MESSAGE "nox: runtime error: division by zero\\n"

static_assert(SLOT_SIZE == sizeof(int64_t), "a slot must hold an int64_t");
static_assert((STACK_ALIGNMENT & (STACK_ALIGNMENT - 1)) == 0,
              "the stack alignment must be a power of two");

typedef struct codegen
{
    FILE*    out;
    unsigned labels;
} codegen_t;

static size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static size_t slot_offset(size_t slot)
{
    return (slot + 1) * SLOT_SIZE;
}

static void codegen_node(codegen_t* codegen, const ast_node_t* node);

static void codegen_division(codegen_t* codegen)
{
    unsigned label = codegen->labels++;

    fprintf(codegen->out,
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

static void codegen_binary(codegen_t* codegen, const ast_node_t* node)
{
    codegen_node(codegen, node->binary.left);
    fprintf(codegen->out, "    push rax\n");

    codegen_node(codegen, node->binary.right);
    fprintf(codegen->out,
            "    mov rcx, rax\n"
            "    pop rax\n");

    switch (node->binary.op)
    {
    case BINARY_OP_ADD:
        fprintf(codegen->out, "    add rax, rcx\n");
        break;

    case BINARY_OP_SUB:
        fprintf(codegen->out, "    sub rax, rcx\n");
        break;

    case BINARY_OP_MUL:
        fprintf(codegen->out, "    imul rax, rcx\n");
        break;

    case BINARY_OP_DIV:
        codegen_division(codegen);
        break;
    }
}

static void codegen_node(codegen_t* codegen, const ast_node_t* node)
{
    switch (node->kind)
    {
    case AST_INTEGER:
        fprintf(codegen->out, "    mov rax, %" PRId64 "\n", node->integer);
        break;

    case AST_IDENTIFIER:
        fprintf(codegen->out,
                "    mov rax, [rbp - %zu]\n",
                slot_offset(node->slot));
        break;

    case AST_BINARY:
        codegen_binary(codegen, node);
        break;

    case AST_UNARY:
        codegen_node(codegen, node->unary.operand);

        switch (node->unary.op)
        {
        case UNARY_OP_NEG:
            fprintf(codegen->out, "    neg rax\n");
            break;
        }
        break;

    case AST_LET:
        codegen_node(codegen, node->let.value);
        fprintf(codegen->out,
                "    mov [rbp - %zu], rax\n",
                slot_offset(node->slot));
        break;

    case AST_PRINT:
        codegen_node(codegen, node->print.value);
        fprintf(codegen->out,
                "    mov rsi, rax\n"
                "    lea rdi, [rip + .Lfmt]\n"
                "    xor eax, eax\n"
                "    call printf@PLT\n");
        break;

    case AST_PROGRAM:
        for (size_t i = 0; i < node->program.count; i++)
            codegen_node(codegen, node->program.statements[i]);
        break;
    }
}

bool codegen_emit(FILE* out, const ast_node_t* program, size_t slot_count)
{
#if !defined(__x86_64__) || !defined(__linux__)
#error "nox currently targets x86-64 Linux only"
#endif
    codegen_t codegen = {.out = out, .labels = 0};

    size_t frame_size = align_up(slot_count * SLOT_SIZE, STACK_ALIGNMENT);

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

    if (frame_size > 0)
        fprintf(out, "    sub rsp, %zu\n", frame_size);

    codegen_node(&codegen, program);

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
