#include <stdlib.h>
#include "function.h"
#include "util.h"

Function *function_new(const char *name, int length)
{
	Function *f = xmalloc(sizeof *f);
	f->refs = 1;
	f->name = xmalloc((size_t)length + 1);
	for (int i = 0; i < length; i++)
		f->name[i] = name[i];
	f->name[length] = '\0';
	f->arity = 0;
	f->nlocals = 0;
	chunk_init(&f->chunk);
	return f;
}

void function_release(Function *f)
{
	if (--f->refs > 0)
		return;
	chunk_free(&f->chunk);
	free(f->name);
	free(f);
}
