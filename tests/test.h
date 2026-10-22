#ifndef TEST_H
#define TEST_H

#include <stdio.h>

static int test_failures = 0;

#define ASSERT(cond)                                                           \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            fprintf(stderr,                                                    \
                    "%s:%d ASSERT failed: %s\n",                               \
                    __FILE__,                                                  \
                    __LINE__,                                                  \
                    #cond);                                                    \
            test_failures++;                                                   \
        }                                                                      \
    } while (0)

#define TEST_EXIT() return test_failures ? 1 : 0

#endif
