#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "opcode.h"

static bool stmts(Chunk *c, Stmt *s);

static bool fail(int line, const char *msg)
{
	fprintf(stderr, "line %d: %s\n", line, msg);
	return false;
}

static bool constant(Chunk *c, Value v, int line, OpCode op)
{
	int idx = -1;
	for (int i = 0; i < c->constants.count; i++) {
		if (c->constants.values[i].type == v.type &&
		    value_equals(c->constants.values[i], v)) {
			idx = i;
			value_free(v);
			break;
		}
	}
	if (idx < 0) {
		if (c->constants.count >= 256) {
			value_free(v);
			return fail(line, "too many constants");
		}
		idx = chunk_add_constant(c, v);
	}
	chunk_write(c, op, line);
	chunk_write(c, (uint8_t)idx, line);
	return true;
}

static bool name_op(Chunk *c, const char *name, int length, int line, OpCode op)
{
	return constant(c, value_string(name, length), line, op);
}

static int emit_jump(Chunk *c, OpCode op, int line)
{
	chunk_write(c, op, line);
	chunk_write(c, 0xff, line);
	chunk_write(c, 0xff, line);
	return c->count - 2;
}

static bool patch_jump(Chunk *c, int at, int line)
{
	int off = c->count - (at + 2);
	if (off > 65535)
		return fail(line, "jump too large");
	c->code[at] = (uint8_t)(off >> 8);
	c->code[at + 1] = (uint8_t)(off & 0xff);
	return true;
}

static bool emit_loop(Chunk *c, int start, int line)
{
	int off = c->count + 3 - start;
	if (off > 65535)
		return fail(line, "loop body too large");
	chunk_write(c, OP_LOOP, line);
	chunk_write(c, (uint8_t)(off >> 8), line);
	chunk_write(c, (uint8_t)(off & 0xff), line);
	return true;
}

static bool expr(Chunk *c, Expr *e)
{
	int line = e->line;
	switch (e->type) {
	case EXPR_LITERAL: {
		Value v = e->as.literal;
		if (v.type == VAL_STRING)
			v = value_string(v.as.s, (int)strlen(v.as.s));
		return constant(c, v, line, OP_CONST);
	}
	case EXPR_NAME:
		return name_op(c, e->as.name.name, e->as.name.length, line, OP_GET_GLOBAL);
	case EXPR_UNARY:
		if (!expr(c, e->as.unary.right))
			return false;
		switch (e->as.unary.op) {
		case TOK_MINUS: chunk_write(c, OP_NEG, line); return true;
		case TOK_NOT: chunk_write(c, OP_NOT, line); return true;
		default: return fail(line, "bad unary operator");
		}
	case EXPR_BINARY: {
		OpCode op;
		switch (e->as.binary.op) {
		case TOK_PLUS: op = OP_ADD; break;
		case TOK_MINUS: op = OP_SUB; break;
		case TOK_STAR: op = OP_MUL; break;
		case TOK_SLASH: op = OP_DIV; break;
		case TOK_PERCENT: op = OP_MOD; break;
		case TOK_EQEQ: op = OP_EQ; break;
		case TOK_NE: op = OP_NE; break;
		case TOK_LT: op = OP_LT; break;
		case TOK_LE: op = OP_LE; break;
		case TOK_GT: op = OP_GT; break;
		case TOK_GE: op = OP_GE; break;
		default: return fail(line, "bad binary operator");
		}
		if (!expr(c, e->as.binary.left) || !expr(c, e->as.binary.right))
			return false;
		chunk_write(c, op, line);
		return true;
	}
	case EXPR_LOGICAL: {
		if (!expr(c, e->as.logical.left))
			return false;
		if (e->as.logical.op == TOK_AND) {
			int end = emit_jump(c, OP_JUMP_IF_FALSE, line);
			chunk_write(c, OP_POP, line);
			if (!expr(c, e->as.logical.right))
				return false;
			return patch_jump(c, end, line);
		}
		int other = emit_jump(c, OP_JUMP_IF_FALSE, line);
		int end = emit_jump(c, OP_JUMP, line);
		if (!patch_jump(c, other, line))
			return false;
		chunk_write(c, OP_POP, line);
		if (!expr(c, e->as.logical.right))
			return false;
		return patch_jump(c, end, line);
	}
	}
	return fail(line, "bad expression");
}

static bool stmt(Chunk *c, Stmt *s)
{
	int line = s->line;
	switch (s->type) {
	case STMT_EXPR:
		if (!expr(c, s->as.expr))
			return false;
		chunk_write(c, OP_POP, line);
		return true;
	case STMT_PRINT:
		if (!expr(c, s->as.expr))
			return false;
		chunk_write(c, OP_PRINT, line);
		return true;
	case STMT_ASSIGN:
		if (!expr(c, s->as.assign.value))
			return false;
		return name_op(c, s->as.assign.name, s->as.assign.length, line, OP_SET_GLOBAL);
	case STMT_IF: {
		if (!expr(c, s->as.if_stmt.cond))
			return false;
		int else_j = emit_jump(c, OP_JUMP_IF_FALSE, line);
		chunk_write(c, OP_POP, line);
		if (!stmts(c, s->as.if_stmt.then_branch))
			return false;
		int end_j = emit_jump(c, OP_JUMP, line);
		if (!patch_jump(c, else_j, line))
			return false;
		chunk_write(c, OP_POP, line);
		if (!stmts(c, s->as.if_stmt.else_branch))
			return false;
		return patch_jump(c, end_j, line);
	}
	case STMT_WHILE: {
		int start = c->count;
		if (!expr(c, s->as.while_stmt.cond))
			return false;
		int exit_j = emit_jump(c, OP_JUMP_IF_FALSE, line);
		chunk_write(c, OP_POP, line);
		if (!stmts(c, s->as.while_stmt.body) || !emit_loop(c, start, line))
			return false;
		if (!patch_jump(c, exit_j, line))
			return false;
		chunk_write(c, OP_POP, line);
		return true;
	}
	}
	return fail(line, "bad statement");
}

static bool stmts(Chunk *c, Stmt *s)
{
	for (; s; s = s->next)
		if (!stmt(c, s))
			return false;
	return true;
}

bool compile(Program *prog, Chunk *chunk)
{
	if (!stmts(chunk, prog->body))
		return false;
	chunk_write(chunk, OP_HALT, chunk->count ? chunk->lines[chunk->count - 1] : 0);
	return true;
}
