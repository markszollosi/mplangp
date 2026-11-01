#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>
#include "value.h"

typedef struct {
	uint8_t *code;
	int *lines;
	int count, capacity;
	ValueArray constants;
} Chunk;

void chunk_init(Chunk *c);
void chunk_free(Chunk *c);
void chunk_write(Chunk *c, uint8_t byte, int line);
int chunk_add_constant(Chunk *c, Value v);

#endif
