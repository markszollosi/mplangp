#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../chunk.h"
#include "../opcode.h"
#include "../util.h"

#ifdef STUBS
void *xmalloc(size_t n) { void *p = malloc(n); assert(p); return p; }
void *xrealloc(void *p, size_t n) { p = realloc(p, n); assert(p); return p; }
void die(const char *fmt, ...) { (void)fmt; abort(); }
void value_array_init(ValueArray *a) { a->values = NULL; a->count = a->capacity = 0; }
void value_array_write(ValueArray *a, Value v)
{
	if (a->count == a->capacity) {
		a->capacity = a->capacity ? a->capacity * 2 : 8;
		a->values = xrealloc(a->values, a->capacity * sizeof *a->values);
	}
	a->values[a->count++] = v;
}
void value_array_free(ValueArray *a)
{
	for (int i = 0; i < a->count; i++) {
		if (a->values[i].type == VAL_STRING) free(a->values[i].as.s);
	}
	free(a->values);
	value_array_init(a);
}
Value value_int(int64_t i) { Value v; v.type = VAL_INT; v.as.i = i; return v; }
Value value_string(const char *s, int n)
{
	Value v;
	v.type = VAL_STRING;
	v.as.s = xmalloc(n + 1);
	memcpy(v.as.s, s, n);
	v.as.s[n] = 0;
	return v;
}
#endif

int main(void)
{
	Chunk c;
	chunk_init(&c);
	assert(c.count == 0);
	chunk_free(&c);
	chunk_init(&c);
	for (int i = 0; i < 1000; i++) chunk_write(&c, (uint8_t)(i % 23), i + 1);
	assert(c.count == 1000 && c.capacity >= 1000);
	for (int i = 0; i < 1000; i++) {
		assert(c.code[i] == i % 23);
		assert(c.lines[i] == i + 1);
	}
	assert(chunk_add_constant(&c, value_int(42)) == 0);
	assert(chunk_add_constant(&c, value_string("hi", 2)) == 1);
	assert(c.constants.values[0].as.i == 42);
	assert(strcmp(c.constants.values[1].as.s, "hi") == 0);
	chunk_free(&c);
	assert(c.code == NULL && c.count == 0);
	chunk_init(&c);
	chunk_write(&c, OP_HALT, 1);
	assert(c.code[0] == OP_HALT);
	chunk_free(&c);
	puts("ok");
	return 0;
}
