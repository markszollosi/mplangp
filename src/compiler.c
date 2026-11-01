#include <stdio.h>
#include <string.h>
#include "compiler.h"
#include "opcode.h"
#include "function.h"

static bool stmts(Chunk *c, Stmt *s);

#define LOCALS_MAX 255

static struct {
	const char *name[LOCALS_MAX];
	int length[LOCALS_MAX];
	int n;
	bool infunc;
} scope;

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

#define LOOPS_MAX 32
#define BREAKS_MAX 256

static struct {
	int start[LOOPS_MAX]; /* code offset of each open loop's condition */
	int mark[LOOPS_MAX];  /* breaks[] length when the loop was entered */
	int depth;
	int floor;            /* loops below this belong to an enclosing function */
	int breaks[BREAKS_MAX]; /* pending break jumps awaiting a target */
	int nbreaks;
} loops;

static int local_slot(const char *name, int length)
{
	if (!scope.infunc)
		return -1;
	for (int i = 0; i < scope.n; i++)
		if (scope.length[i] == length && memcmp(scope.name[i], name, (size_t)length) == 0)
			return i + 1;
	return -1;
}

static bool add_local(const char *name, int length, int line)
{
	if (local_slot(name, length) >= 0)
		return true;
	if (scope.n >= LOCALS_MAX)
		return fail(line, "too many local variables");
	scope.name[scope.n] = name;
	scope.length[scope.n++] = length;
	return true;
}

#define FORS_MAX 16
#define H(n) { "$it" #n, "$ix" #n }
static const char *const hidden[FORS_MAX][2] = {
	H(0), H(1), H(2), H(3), H(4), H(5), H(6), H(7),
	H(8), H(9), H(10), H(11), H(12), H(13), H(14), H(15)
};
static int fordepth;

static bool collect_locals(Stmt *s, int fd)
{
	for (; s; s = s->next) {
		switch (s->type) {
		case STMT_ASSIGN:
			if (!add_local(s->as.assign.name, s->as.assign.length, s->line))
				return false;
			break;
		case STMT_IF:
			if (!collect_locals(s->as.if_stmt.then_branch, fd) || !collect_locals(s->as.if_stmt.else_branch, fd))
				return false;
			break;
		case STMT_WHILE:
			if (!collect_locals(s->as.while_stmt.body, fd))
				return false;
			break;
		case STMT_FOR:
			if (fd >= FORS_MAX)
				return fail(s->line, "for loops nested too deeply");
			if (!add_local(s->as.for_stmt.name, s->as.for_stmt.length, s->line) ||
			    !add_local(hidden[fd][0], (int)strlen(hidden[fd][0]), s->line) ||
			    !add_local(hidden[fd][1], (int)strlen(hidden[fd][1]), s->line) ||
			    !collect_locals(s->as.for_stmt.body, fd + 1))
				return false;
			break;
		default:
			break;
		}
	}
	return true;
}

static bool var_op(Chunk *c, const char *name, int length, int line, bool set)
{
	int slot = local_slot(name, length);
	if (slot < 0)
		return name_op(c, name, length, line, set ? OP_SET_GLOBAL : OP_GET_GLOBAL);
	chunk_write(c, set ? OP_SET_LOCAL : OP_GET_LOCAL, line);
	chunk_write(c, (uint8_t)slot, line);
	return true;
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
		return var_op(c, e->as.name.name, e->as.name.length, line, false);
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
		case TOK_IN: op = OP_IN; break;
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
	case EXPR_CALL:
		if (e->as.call.nargs > 255)
			return fail(line, "too many arguments");
		if (!expr(c, e->as.call.callee))
			return false;
		for (int i = 0; i < e->as.call.nargs; i++)
			if (!expr(c, e->as.call.args[i]))
				return false;
		chunk_write(c, OP_CALL, line);
		chunk_write(c, (uint8_t)e->as.call.nargs, line);
		return true;
	case EXPR_ATTR:
		if (!expr(c, e->as.attr.object))
			return false;
		return name_op(c, e->as.attr.name, e->as.attr.length, line, OP_GET_ATTR);
	case EXPR_LIST:
		if (e->as.list.n > 255)
			return fail(line, "list literal too long");
		for (int i = 0; i < e->as.list.n; i++)
			if (!expr(c, e->as.list.items[i]))
				return false;
		chunk_write(c, OP_LIST, line);
		chunk_write(c, (uint8_t)e->as.list.n, line);
		return true;
	case EXPR_MAP:
		if (e->as.map.n > 255)
			return fail(line, "map literal too long");
		for (int i = 0; i < e->as.map.n; i++)
			if (!expr(c, e->as.map.keys[i]) || !expr(c, e->as.map.vals[i]))
				return false;
		chunk_write(c, OP_MAP, line);
		chunk_write(c, (uint8_t)e->as.map.n, line);
		return true;
	case EXPR_INDEX:
		if (!expr(c, e->as.index.object) || !expr(c, e->as.index.index))
			return false;
		chunk_write(c, OP_INDEX, line);
		return true;
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
		return var_op(c, s->as.assign.name, s->as.assign.length, line, true);
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
		if (loops.depth >= LOOPS_MAX)
			return fail(line, "loops nested too deeply");
		loops.start[loops.depth] = start;
		loops.mark[loops.depth++] = loops.nbreaks;
		if (!expr(c, s->as.while_stmt.cond))
			return false;
		int exit_j = emit_jump(c, OP_JUMP_IF_FALSE, line);
		chunk_write(c, OP_POP, line);
		if (!stmts(c, s->as.while_stmt.body) || !emit_loop(c, start, line))
			return false;
		if (!patch_jump(c, exit_j, line))
			return false;
		chunk_write(c, OP_POP, line);
		loops.depth--;
		while (loops.nbreaks > loops.mark[loops.depth])
			if (!patch_jump(c, loops.breaks[--loops.nbreaks], line))
				return false;
		return true;
	}
	case STMT_SETINDEX:
		if (!expr(c, s->as.setindex.object) || !expr(c, s->as.setindex.index) || !expr(c, s->as.setindex.value))
			return false;
		chunk_write(c, OP_SET_INDEX, line);
		return true;
	case STMT_FOR: {
		if (loops.depth >= LOOPS_MAX)
			return fail(line, "loops nested too deeply");
		if (fordepth >= FORS_MAX)
			return fail(line, "for loops nested too deeply");
		const char *it = hidden[fordepth][0], *ix = hidden[fordepth][1];
		int itlen = (int)strlen(it), ixlen = (int)strlen(ix);
		if (!expr(c, s->as.for_stmt.iter) || !var_op(c, it, itlen, line, true) ||
		    !constant(c, value_int(0), line, OP_CONST) || !var_op(c, ix, ixlen, line, true))
			return false;
		int start = c->count;
		loops.start[loops.depth] = start;
		loops.mark[loops.depth++] = loops.nbreaks;
		fordepth++;
		if (!var_op(c, it, itlen, line, false) || !var_op(c, ix, ixlen, line, false))
			return false;
		chunk_write(c, OP_ITER_HAS, line);
		int exit_j = emit_jump(c, OP_JUMP_IF_FALSE, line);
		chunk_write(c, OP_POP, line);
		if (!var_op(c, it, itlen, line, false) || !var_op(c, ix, ixlen, line, false))
			return false;
		chunk_write(c, OP_ITER_GET, line);
		if (!var_op(c, s->as.for_stmt.name, s->as.for_stmt.length, line, true) ||
		    !var_op(c, ix, ixlen, line, false) || !constant(c, value_int(1), line, OP_CONST))
			return false;
		chunk_write(c, OP_ADD, line);
		if (!var_op(c, ix, ixlen, line, true) || !stmts(c, s->as.for_stmt.body) || !emit_loop(c, start, line))
			return false;
		if (!patch_jump(c, exit_j, line))
			return false;
		chunk_write(c, OP_POP, line);
		fordepth--;
		loops.depth--;
		while (loops.nbreaks > loops.mark[loops.depth])
			if (!patch_jump(c, loops.breaks[--loops.nbreaks], line))
				return false;
		return true;
	}
	case STMT_BREAK:
		if (loops.depth <= loops.floor)
			return fail(line, "'break' outside loop");
		if (loops.nbreaks >= BREAKS_MAX)
			return fail(line, "too many break statements");
		loops.breaks[loops.nbreaks++] = emit_jump(c, OP_JUMP, line);
		return true;
	case STMT_CONTINUE:
		if (loops.depth <= loops.floor)
			return fail(line, "'continue' outside loop");
		return emit_loop(c, loops.start[loops.depth - 1], line);
	case STMT_RETURN:
		if (s->as.expr) {
			if (!expr(c, s->as.expr))
				return false;
		} else if (!constant(c, value_none(), line, OP_CONST)) {
			return false;
		}
		chunk_write(c, OP_RETURN, line);
		return true;
	case STMT_IMPORT:
		return name_op(c, s->as.import.name, s->as.import.length, line, OP_IMPORT) &&
		    name_op(c, s->as.import.name, s->as.import.length, line, OP_SET_GLOBAL);
	case STMT_DEF: {
		if (scope.infunc)
			return fail(line, "nested def is not supported");
		Function *f = function_new(s->as.def.name, s->as.def.length);
		f->arity = s->as.def.nparams;
		scope.n = 0;
		scope.infunc = true;
		int saved_floor = loops.floor, saved_for = fordepth;
		loops.floor = loops.depth;
		fordepth = 0;
		bool ok = true;
		for (int i = 0; ok && i < s->as.def.nparams; i++) {
			Expr *p = s->as.def.params[i];
			if (local_slot(p->as.name.name, p->as.name.length) >= 0)
				ok = fail(p->line, "duplicate parameter");
			else
				ok = add_local(p->as.name.name, p->as.name.length, p->line);
		}
		ok = ok && collect_locals(s->as.def.body, 0);
		f->nlocals = scope.n;
		ok = ok && stmts(&f->chunk, s->as.def.body) &&
		    constant(&f->chunk, value_none(), line, OP_CONST);
		if (ok)
			chunk_write(&f->chunk, OP_RETURN, line);
		scope.infunc = false;
		scope.n = 0;
		loops.floor = saved_floor;
		fordepth = saved_for;
		if (!ok) {
			function_release(f);
			return false;
		}
		Value fv;
		fv.type = VAL_FUNC;
		fv.as.fn = f;
		return constant(c, fv, line, OP_CONST) &&
		    name_op(c, s->as.def.name, s->as.def.length, line, OP_SET_GLOBAL);
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
	scope.infunc = false;
	scope.n = 0;
	loops.depth = loops.floor = loops.nbreaks = 0;
	fordepth = 0;
	if (!stmts(chunk, prog->body))
		return false;
	chunk_write(chunk, OP_HALT, chunk->count ? chunk->lines[chunk->count - 1] : 0);
	return true;
}
