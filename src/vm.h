#ifndef VM_H
#define VM_H

#include "chunk.h"

typedef enum { INTERPRET_OK, INTERPRET_RUNTIME_ERROR } InterpretResult;

void vm_init(void);
void vm_free(void);
InterpretResult vm_run(Chunk *chunk);

#endif
