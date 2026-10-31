#include "diagnostic.h"

#include "lexer.h"

#include <stddef.h>
#include <stdio.h>

static int decimal_width(size_t n)
{
    int width = 1;

    while (n >= 10)
    {
        n /= 10;
        width++;
    }

    return width;
}

source_location_t source_locate(source_t source, size_t offset)
{
    if (offset > source.length)
        offset = source.length;

    size_t line       = 1;
    size_t line_start = 0;

    for (size_t i = 0; i < offset; i++)
    {
        if (source.data[i] == '\n')
        {
            line++;
            line_start = i + 1;
        }
    }

    size_t line_end = offset;
    while (line_end < source.length && source.data[line_end] != '\n')
        line_end++;

    if (line_end > offset && source.data[line_end - 1] == '\r')
        line_end--;

    return (source_location_t){
        .line       = line,
        .column     = offset - line_start + 1,
        .line_start = line_start,
        .line_end   = line_end,
    };
}

void diagnostic_print(FILE*               out,
                      const char*         filename,
                      source_t            source,
                      const diagnostic_t* diagnostic)
{
    size_t            offset = diagnostic->span.start;
    source_location_t loc    = source_locate(source, offset);

    if (offset > source.length)
        offset = source.length;

    fprintf(out,
            "%s:%zu:%zu: error: %s\n",
            filename,
            loc.line,
            loc.column,
            diagnostic->message);

    int gutter = decimal_width(loc.line);

    fprintf(out, "%*zu | ", gutter, loc.line);
    fwrite(source.data + loc.line_start, 1, loc.line_end - loc.line_start, out);
    fputc('\n', out);

    fprintf(out, "%*s | ", gutter, "");

    for (size_t i = loc.line_start; i < offset; i++)
        fputc(source.data[i] == '\t' ? '\t' : ' ', out);

    size_t end = diagnostic->span.end;
    if (end > loc.line_end)
        end = loc.line_end;

    size_t length = end > offset ? end - offset : 1;

    fputc('^', out);
    for (size_t i = 1; i < length; i++)
        fputc('~', out);

    fputc('\n', out);
}
