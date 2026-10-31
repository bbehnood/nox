#define _POSIX_C_SOURCE 200809L

#include "ast.h"
#include "codegen.h"
#include "diagnostic.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static constexpr size_t READ_INITIAL_CAPACITY      = 4096;
static constexpr size_t COMMAND_RESERVED_LENGTH    = 32;
static constexpr int    EXIT_INVALID_INPUT         = 2;
static constexpr int    SHELL_EXIT_COMMAND_MISSING = 127;

static const char DEFAULT_COMPILER[]    = "cc";
static const char DEFAULT_OUTPUT_PATH[] = "a.out";

static char* read_file(const char* path, size_t* length)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return nullptr;

    size_t capacity = READ_INITIAL_CAPACITY;
    size_t size     = 0;
    char*  data     = malloc(capacity);

    while (data)
    {
        size += fread(data + size, 1, capacity - size - 1, file);

        if (size < capacity - 1)
            break;

        char* grown = realloc(data, capacity * 2);
        if (!grown)
        {
            free(data);
            data = nullptr;
            break;
        }

        data = grown;
        capacity *= 2;
    }

    int failed = ferror(file);
    fclose(file);

    if (data && failed)
    {
        free(data);
        return nullptr;
    }

    if (data)
    {
        data[size] = '\0';
        *length    = size;
    }

    return data;
}

typedef struct options
{
    const char* input;
    const char* output;
    bool        emit_asm;
} options_t;

static void print_usage(const char* program)
{
    fprintf(stderr,
            "usage: %s [-S] [-o <output>] <file>\n"
            "  -o <output>  write the executable (or assembly with -S) here\n"
            "  -S           emit assembly instead of an executable\n",
            program);
}

static bool parse_args(int argc, char** argv, options_t* options)
{
    *options = (options_t){};

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-S") == 0)
        {
            options->emit_asm = true;
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (++i >= argc)
                return false;

            options->output = argv[i];
        }
        else if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            return false;
        }
        else if (options->input)
        {
            return false;
        }
        else
        {
            options->input = argv[i];
        }
    }

    return options->input != nullptr;
}

static char* shell_quote(const char* text)
{
    size_t length = 2;

    for (const char* c = text; *c; c++)
        length += *c == '\'' ? 4 : 1;

    char* quoted = malloc(length + 1);
    if (!quoted)
        return nullptr;

    char* out = quoted;
    *out++    = '\'';

    for (const char* c = text; *c; c++)
    {
        if (*c == '\'')
        {
            memcpy(out, "'\\''", 4);
            out += 4;
        }
        else
        {
            *out++ = *c;
        }
    }

    *out++ = '\'';
    *out   = '\0';

    return quoted;
}

static bool emit_executable(const ast_node_t* program,
                            size_t            slot_count,
                            const char*       output)
{
    const char* cc = getenv("CC");
    if (!cc || !*cc)
        cc = DEFAULT_COMPILER;

    char* quoted = shell_quote(output);
    if (!quoted)
        return false;

    size_t size    = strlen(cc) + strlen(quoted) + COMMAND_RESERVED_LENGTH;
    char*  command = malloc(size);

    if (!command)
    {
        free(quoted);
        return false;
    }

    snprintf(command, size, "%s -x assembler -o %s -", cc, quoted);

    FILE* pipe = popen(command, "w");

    free(command);
    free(quoted);

    if (!pipe)
        return false;

    void (*old_handler)(int) = signal(SIGPIPE, SIG_IGN);

    bool emitted = codegen_emit(pipe, program, slot_count);
    int  status  = pclose(pipe);

    signal(SIGPIPE, old_handler);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        if (WIFEXITED(status) &&
            WEXITSTATUS(status) == SHELL_EXIT_COMMAND_MISSING)
            fprintf(stderr, "nox: error: could not run C compiler '%s'\n", cc);
        else
            fprintf(stderr, "nox: error: C compiler '%s' failed\n", cc);
        return false;
    }

    return emitted;
}

static bool emit_assembly(const ast_node_t* program,
                          size_t            slot_count,
                          const char*       output)
{
    FILE* file = output ? fopen(output, "w") : stdout;
    if (!file)
        return false;

    bool emitted = codegen_emit(file, program, slot_count);

    if (output && fclose(file) != 0)
        emitted = false;

    return emitted;
}

int main(int argc, char** argv)
{
    options_t options;

    if (!parse_args(argc, argv, &options))
    {
        print_usage(argc > 0 ? argv[0] : "nox");
        return EXIT_INVALID_INPUT;
    }

    const char* path   = options.input;
    size_t      length = 0;
    char*       text   = read_file(path, &length);

    if (!text)
    {
        fprintf(stderr, "%s: error: cannot read file\n", path);
        return EXIT_INVALID_INPUT;
    }

    source_t source = {.data = text, .length = length};

    parser_t parser;
    parser_init(&parser, source);

    ast_node_t* program = parser_parse(&parser);

    int status = EXIT_SUCCESS;

    if (!program)
    {
        const diagnostic_t* error = parser_error(&parser);

        if (error)
            diagnostic_print(stderr, path, source, error);
        else
            fprintf(stderr, "%s: error: parsing failed\n", path);

        status = EXIT_FAILURE;
    }
    else
    {
        sema_t sema;
        sema_init(&sema, source);

        if (!sema_check(&sema, program))
        {
            diagnostic_print(stderr, path, source, sema_error(&sema));
            status = EXIT_FAILURE;
        }
        else
        {
            bool ok =
                options.emit_asm
                    ? emit_assembly(program, sema.slot_count, options.output)
                    : emit_executable(program,
                                      sema.slot_count,
                                      options.output ? options.output
                                                     : DEFAULT_OUTPUT_PATH);

            if (!ok)
            {
                fprintf(stderr, "%s: error: code generation failed\n", path);
                status = EXIT_FAILURE;
            }
        }

        sema_free(&sema);
    }

    ast_free(program);
    free(text);

    return status;
}
