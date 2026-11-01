#include <stdlib.h>

#include "ast.h"
#include "util.h"

static Expr *new_expr(ExprType type, int line)
{
	Expr *e = xmalloc(sizeof *e);
	e->type = type;
	e->line = line;
	return e;
}

static Stmt *new_stmt(StmtType type, int line)
{
	Stmt *s = xmalloc(sizeof *s);
	s->type = type;
	s->line = line;
	s->next = NULL;
	return s;
}

Expr *ast_new_literal(Value v, int line)
{
	Expr *e = new_expr(EXPR_LITERAL, line);
	e->as.literal = v;
	return e;
}

Expr *ast_new_name(const char *name, int length, int line)
{
	Expr *e = new_expr(EXPR_NAME, line);
	e->as.name.name = name;
	e->as.name.length = length;
	return e;
}

Expr *ast_new_unary(TokenType op, Expr *right, int line)
{
	Expr *e = new_expr(EXPR_UNARY, line);
	e->as.unary.op = op;
	e->as.unary.right = right;
	return e;
}

Expr *ast_new_binary(TokenType op, Expr *left, Expr *right, int line)
{
	Expr *e = new_expr(EXPR_BINARY, line);
	e->as.binary.op = op;
	e->as.binary.left = left;
	e->as.binary.right = right;
	return e;
}

Expr *ast_new_logical(TokenType op, Expr *left, Expr *right, int line)
{
	Expr *e = new_expr(EXPR_LOGICAL, line);
	e->as.logical.op = op;
	e->as.logical.left = left;
	e->as.logical.right = right;
	return e;
}

Expr *ast_new_call(Expr *callee, Expr **args, int nargs, int line)
{
	Expr *e = new_expr(EXPR_CALL, line);
	e->as.call.callee = callee;
	e->as.call.args = args;
	e->as.call.nargs = nargs;
	return e;
}

Expr *ast_new_attr(Expr *object, const char *name, int length, int line)
{
	Expr *e = new_expr(EXPR_ATTR, line);
	e->as.attr.object = object;
	e->as.attr.name = name;
	e->as.attr.length = length;
	return e;
}

Expr *ast_new_list(Expr **items, int n, int line)
{
	Expr *e = new_expr(EXPR_LIST, line);
	e->as.list.items = items;
	e->as.list.n = n;
	return e;
}

Expr *ast_new_map(Expr **keys, Expr **vals, int n, int line)
{
	Expr *e = new_expr(EXPR_MAP, line);
	e->as.map.keys = keys;
	e->as.map.vals = vals;
	e->as.map.n = n;
	return e;
}

Expr *ast_new_index(Expr *object, Expr *index, int line)
{
	Expr *e = new_expr(EXPR_INDEX, line);
	e->as.index.object = object;
	e->as.index.index = index;
	return e;
}

Stmt *ast_new_for_stmt(const char *name, int length, Expr *iter, Stmt *body, int line)
{
	Stmt *s = new_stmt(STMT_FOR, line);
	s->as.for_stmt.name = name;
	s->as.for_stmt.length = length;
	s->as.for_stmt.iter = iter;
	s->as.for_stmt.body = body;
	return s;
}

Stmt *ast_new_setindex_stmt(Expr *object, Expr *index, Expr *value, int line)
{
	Stmt *s = new_stmt(STMT_SETINDEX, line);
	s->as.setindex.object = object;
	s->as.setindex.index = index;
	s->as.setindex.value = value;
	return s;
}

Stmt *ast_new_def_stmt(const char *name, int length, Expr **params, int nparams, Stmt *body, int line)
{
	Stmt *s = new_stmt(STMT_DEF, line);
	s->as.def.name = name;
	s->as.def.length = length;
	s->as.def.params = params;
	s->as.def.nparams = nparams;
	s->as.def.body = body;
	return s;
}

Stmt *ast_new_return_stmt(Expr *e, int line)
{
	Stmt *s = new_stmt(STMT_RETURN, line);
	s->as.expr = e;
	return s;
}

Stmt *ast_new_break_stmt(int line)
{
	return new_stmt(STMT_BREAK, line);
}

Stmt *ast_new_continue_stmt(int line)
{
	return new_stmt(STMT_CONTINUE, line);
}

Stmt *ast_new_import_stmt(const char *name, int length, int line)
{
	Stmt *s = new_stmt(STMT_IMPORT, line);
	s->as.import.name = name;
	s->as.import.length = length;
	return s;
}

Stmt *ast_new_expr_stmt(Expr *e, int line)
{
	Stmt *s = new_stmt(STMT_EXPR, line);
	s->as.expr = e;
	return s;
}

Stmt *ast_new_print_stmt(Expr *e, int line)
{
	Stmt *s = new_stmt(STMT_PRINT, line);
	s->as.expr = e;
	return s;
}

Stmt *ast_new_assign_stmt(const char *name, int length, Expr *value, int line)
{
	Stmt *s = new_stmt(STMT_ASSIGN, line);
	s->as.assign.name = name;
	s->as.assign.length = length;
	s->as.assign.value = value;
	return s;
}

Stmt *ast_new_if_stmt(Expr *cond, Stmt *then_branch, Stmt *else_branch, int line)
{
	Stmt *s = new_stmt(STMT_IF, line);
	s->as.if_stmt.cond = cond;
	s->as.if_stmt.then_branch = then_branch;
	s->as.if_stmt.else_branch = else_branch;
	return s;
}

Stmt *ast_new_while_stmt(Expr *cond, Stmt *body, int line)
{
	Stmt *s = new_stmt(STMT_WHILE, line);
	s->as.while_stmt.cond = cond;
	s->as.while_stmt.body = body;
	return s;
}

void expr_free(Expr *e)
{
	if (!e) {
		return;
	}
	switch (e->type) {
	case EXPR_LITERAL:
		value_free(e->as.literal);
		break;
	case EXPR_NAME:
		break;
	case EXPR_UNARY:
		expr_free(e->as.unary.right);
		break;
	case EXPR_BINARY:
		expr_free(e->as.binary.left);
		expr_free(e->as.binary.right);
		break;
	case EXPR_LOGICAL:
		expr_free(e->as.logical.left);
		expr_free(e->as.logical.right);
		break;
	case EXPR_CALL:
		expr_free(e->as.call.callee);
		for (int i = 0; i < e->as.call.nargs; i++)
			expr_free(e->as.call.args[i]);
		free(e->as.call.args);
		break;
	case EXPR_ATTR:
		expr_free(e->as.attr.object);
		break;
	case EXPR_LIST:
		for (int i = 0; i < e->as.list.n; i++)
			expr_free(e->as.list.items[i]);
		free(e->as.list.items);
		break;
	case EXPR_MAP:
		for (int i = 0; i < e->as.map.n; i++) {
			expr_free(e->as.map.keys[i]);
			expr_free(e->as.map.vals[i]);
		}
		free(e->as.map.keys);
		free(e->as.map.vals);
		break;
	case EXPR_INDEX:
		expr_free(e->as.index.object);
		expr_free(e->as.index.index);
		break;
	}
	free(e);
}

static void stmt_free(Stmt *s)
{
	while (s) {
		Stmt *next = s->next;
		switch (s->type) {
		case STMT_EXPR:
		case STMT_PRINT:
		case STMT_RETURN:
			expr_free(s->as.expr);
			break;
		case STMT_IMPORT:
		case STMT_BREAK:
		case STMT_CONTINUE:
			break;
		case STMT_DEF:
			for (int i = 0; i < s->as.def.nparams; i++)
				expr_free(s->as.def.params[i]);
			free(s->as.def.params);
			stmt_free(s->as.def.body);
			break;
		case STMT_ASSIGN:
			expr_free(s->as.assign.value);
			break;
		case STMT_IF:
			expr_free(s->as.if_stmt.cond);
			stmt_free(s->as.if_stmt.then_branch);
			stmt_free(s->as.if_stmt.else_branch);
			break;
		case STMT_WHILE:
			expr_free(s->as.while_stmt.cond);
			stmt_free(s->as.while_stmt.body);
			break;
		case STMT_FOR:
			expr_free(s->as.for_stmt.iter);
			stmt_free(s->as.for_stmt.body);
			break;
		case STMT_SETINDEX:
			expr_free(s->as.setindex.object);
			expr_free(s->as.setindex.index);
			expr_free(s->as.setindex.value);
			break;
		}
		free(s);
		s = next;
	}
}

void program_free(Program *p)
{
	if (!p) {
		return;
	}
	stmt_free(p->body);
	free(p);
}
