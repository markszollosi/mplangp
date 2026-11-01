#ifndef COMPILER_H
#define COMPILER_H

#include <stdbool.h>
#include "ast.h"
#include "chunk.h"

bool compile(Program *prog, Chunk *chunk);

#endif
