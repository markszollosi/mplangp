#ifndef OPCODE_H
#define OPCODE_H

/*
 * OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP: followed by a 2-byte big-endian offset.
 * OP_CONST, OP_GET_GLOBAL, OP_SET_GLOBAL, OP_DEFINE_GLOBAL, OP_IMPORT,
 * OP_GET_ATTR: followed by a 1-byte constant-pool index.
 * OP_GET_LOCAL, OP_SET_LOCAL: followed by a 1-byte frame slot (slot 0 is the
 * callee, parameters start at 1). OP_CALL: followed by a 1-byte argument count.
 * OP_WIDE precedes one of those constant-operand opcodes when the pool index
 * exceeds 255 and replaces the 1-byte operand with a 2-byte big-endian one.
 * OP_LIST n / OP_MAP n: followed by a 1-byte element / pair count.
 * OP_ITER_HAS, OP_ITER_GET take [iterable, index] from the stack.
 */
typedef enum {
	OP_CONST, OP_POP,
	OP_GET_GLOBAL, OP_SET_GLOBAL, OP_DEFINE_GLOBAL,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
	OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
	OP_PRINT, OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP, OP_HALT,
	OP_GET_LOCAL, OP_SET_LOCAL, OP_CALL, OP_RETURN, OP_IMPORT, OP_GET_ATTR,
	OP_LIST, OP_MAP, OP_INDEX, OP_SET_INDEX, OP_IN, OP_ITER_HAS, OP_ITER_GET, OP_WIDE
} OpCode;

#endif
