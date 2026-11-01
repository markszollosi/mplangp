#ifndef FUNCTION_H
#define FUNCTION_H

#include "chunk.h"

typedef struct Function {
	int refs;
	char *name;
	int arity;
	int nlocals; /* parameters plus assigned names, excluding the callee slot */
	Chunk chunk;
} Function;

Function *function_new(const char *name, int length); /* refs = 1 */
void function_release(Function *f);

#endif
