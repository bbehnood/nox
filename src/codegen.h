#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"

#include <stddef.h>
#include <stdio.h>

bool codegen_emit(FILE* out, const ast_node_t* program, size_t slot_count);

#endif
