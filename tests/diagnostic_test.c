#include "diagnostic.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

#define SRC(s) ((source_t){.data = (s), .length = strlen(s)})

static void render(char*       out,
                   size_t      size,
                   const char* text,
                   size_t      start,
                   size_t      end,
                   const char* message)
{
    diagnostic_t d;
    d.span = (source_span_t){.start = start, .end = end};
    snprintf(d.message, sizeof(d.message), "%s", message);

    FILE* f = tmpfile();
    ASSERT(f != NULL);
    if (!f)
    {
        out[0] = '\0';
        return;
    }

    diagnostic_print(f, "t.nox", SRC(text), &d);

    rewind(f);
    size_t n = fread(out, 1, size - 1, f);
    out[n]   = '\0';
    fclose(f);
}

static void expect_render(const char* text,
                          size_t      start,
                          size_t      end,
                          const char* expected)
{
    char got[512];
    render(got, sizeof(got), text, start, end, "boom");

    if (strcmp(got, expected) != 0)
        fprintf(stderr, "--- got:\n%s--- want:\n%s---\n", got, expected);
    ASSERT(strcmp(got, expected) == 0);
}

static void test_locate_basic(void)
{
    source_t          s = SRC("ab\ncde\nf");
    source_location_t l;

    l = source_locate(s, 0);
    ASSERT(l.line == 1 && l.column == 1);
    ASSERT(l.line_start == 0 && l.line_end == 2);

    l = source_locate(s, 1);
    ASSERT(l.line == 1 && l.column == 2);

    l = source_locate(s, 2);
    ASSERT(l.line == 1 && l.column == 3);
    ASSERT(l.line_start == 0 && l.line_end == 2);

    l = source_locate(s, 3);
    ASSERT(l.line == 2 && l.column == 1);
    ASSERT(l.line_start == 3 && l.line_end == 6);

    l = source_locate(s, 5);
    ASSERT(l.line == 2 && l.column == 3);

    l = source_locate(s, 7);
    ASSERT(l.line == 3 && l.column == 1);
    ASSERT(l.line_start == 7 && l.line_end == 8);
}

static void test_locate_edges(void)
{
    source_location_t l;

    l = source_locate(SRC(""), 0);
    ASSERT(l.line == 1 && l.column == 1);
    ASSERT(l.line_start == 0 && l.line_end == 0);

    l = source_locate(SRC("a\n"), 2);
    ASSERT(l.line == 2 && l.column == 1);
    ASSERT(l.line_start == 2 && l.line_end == 2);

    l = source_locate(SRC("abc"), 999);
    ASSERT(l.line == 1 && l.column == 4);
    ASSERT(l.line_end == 3);
}

static void test_locate_crlf(void)
{
    source_t          s = SRC("ab\r\ncd");
    source_location_t l = source_locate(s, 0);

    ASSERT(l.line == 1);
    ASSERT(l.line_end == 2);

    l = source_locate(s, 4);
    ASSERT(l.line == 2 && l.column == 1);
    ASSERT(l.line_start == 4 && l.line_end == 6);
}

static void test_render_single_char(void)
{
    expect_render("let x = ;",
                  8,
                  9,
                  "t.nox:1:9: error: boom\n"
                  "1 | let x = ;\n"
                  "  |         ^\n");
}

static void test_render_multi_char_span(void)
{
    expect_render("abcdef",
                  1,
                  4,
                  "t.nox:1:2: error: boom\n"
                  "1 | abcdef\n"
                  "  |  ^~~\n");
}

static void test_render_second_line(void)
{
    expect_render("let x = 1\nprint(x);",
                  10,
                  15,
                  "t.nox:2:1: error: boom\n"
                  "2 | print(x);\n"
                  "  | ^~~~~\n");
}

static void test_render_zero_width_at_end(void)
{
    expect_render("let x = 1",
                  9,
                  9,
                  "t.nox:1:10: error: boom\n"
                  "1 | let x = 1\n"
                  "  |          ^\n");
}

static void test_render_empty_source(void)
{
    expect_render("",
                  0,
                  0,
                  "t.nox:1:1: error: boom\n"
                  "1 | \n"
                  "  | ^\n");
}

static void test_render_span_clamped_to_line(void)
{
    expect_render("ab\ncd",
                  1,
                  5,
                  "t.nox:1:2: error: boom\n"
                  "1 | ab\n"
                  "  |  ^\n");
}

static void test_render_span_past_source(void)
{
    expect_render("abc",
                  1,
                  100,
                  "t.nox:1:2: error: boom\n"
                  "1 | abc\n"
                  "  |  ^~\n");
}

static void test_render_crlf(void)
{
    expect_render("let x = ;\r\nprint(1);",
                  8,
                  9,
                  "t.nox:1:9: error: boom\n"
                  "1 | let x = ;\n"
                  "  |         ^\n");
}

static void test_render_tabs_keep_alignment(void)
{
    expect_render("\tlet x = ;",
                  9,
                  10,
                  "t.nox:1:10: error: boom\n"
                  "1 | \tlet x = ;\n"
                  "  | \t        ^\n");
}

static void test_render_wide_line_numbers(void)
{
    expect_render("\n\n\n\n\n\n\n\n\nx",
                  9,
                  10,
                  "t.nox:10:1: error: boom\n"
                  "10 | x\n"
                  "   | ^\n");
}

int main(void)
{
    test_locate_basic();
    test_locate_edges();
    test_locate_crlf();

    test_render_single_char();
    test_render_multi_char_span();
    test_render_second_line();
    test_render_zero_width_at_end();
    test_render_empty_source();
    test_render_span_clamped_to_line();
    test_render_span_past_source();
    test_render_crlf();
    test_render_tabs_keep_alignment();
    test_render_wide_line_numbers();

    TEST_EXIT();
}
