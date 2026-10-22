#include "ast.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

static char* read_file(const char* path, size_t* length)
{
    FILE* file = fopen(path, "rb");
    if (!file)
        return NULL;

    size_t capacity = 4096;
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
            data = NULL;
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
        return NULL;
    }

    if (data)
    {
        data[size] = '\0';
        *length    = size;
    }

    return data;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <file>\n", argc > 0 ? argv[0] : "nox");
        return 2;
    }

    const char* path   = argv[1];
    size_t      length = 0;
    char*       text   = read_file(path, &length);

    if (!text)
    {
        fprintf(stderr, "%s: error: cannot read file\n", path);
        return 2;
    }

    source_t source = {.data = text, .length = length};

    parser_t parser;
    parser_init(&parser, source);

    ast_node_t* program = parser_parse(&parser);

    int status = 0;

    if (!program)
    {
        const diagnostic_t* error = parser_error(&parser);

        if (error)
            diagnostic_print(stderr, path, source, error);
        else
            fprintf(stderr, "%s: error: parsing failed\n", path);

        status = 1;
    }

    ast_free(program);
    free(text);

    return status;
}
