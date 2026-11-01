#ifndef OPCODE_H
#define OPCODE_H

/*
 * OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP: followed by a 2-byte big-endian offset.
 * OP_CONST, OP_GET_GLOBAL, OP_SET_GLOBAL, OP_DEFINE_GLOBAL, OP_IMPORT,
 * OP_GET_ATTR: followed by a 1-byte constant-pool index.
 * OP_GET_LOCAL, OP_SET_LOCAL: followed by a 1-byte frame slot (slot 0 is the
 * callee, parameters start at 1). OP_CALL: followed by a 1-byte argument count.
 */
typedef enum {
	OP_CONST, OP_POP,
	OP_GET_GLOBAL, OP_SET_GLOBAL, OP_DEFINE_GLOBAL,
	OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_NEG, OP_NOT,
	OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
	OP_PRINT, OP_JUMP, OP_JUMP_IF_FALSE, OP_LOOP, OP_HALT,
	OP_GET_LOCAL, OP_SET_LOCAL, OP_CALL, OP_RETURN, OP_IMPORT, OP_GET_ATTR
} OpCode;

#endif
