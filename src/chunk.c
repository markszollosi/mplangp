#include "chunk.h"
#include <stdlib.h>
#include "util.h"

void chunk_init(Chunk *c)
{
	c->code = NULL;
	c->lines = NULL;
	c->count = 0;
	c->capacity = 0;
	value_array_init(&c->constants);
}

void chunk_free(Chunk *c)
{
	free(c->code);
	free(c->lines);
	value_array_free(&c->constants);
	chunk_init(c);
}

void chunk_write(Chunk *c, uint8_t byte, int line)
{
	if (c->count == c->capacity) {
		c->capacity = c->capacity < 8 ? 8 : c->capacity * 2;
		c->code = xrealloc(c->code, c->capacity * sizeof *c->code);
		c->lines = xrealloc(c->lines, c->capacity * sizeof *c->lines);
	}
	c->code[c->count] = byte;
	c->lines[c->count] = line;
	c->count++;
}

int chunk_add_constant(Chunk *c, Value v)
{
	value_array_write(&c->constants, v);
	return c->constants.count - 1;
}
