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

static void expr_free(Expr *e)
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
			expr_free(s->as.expr);
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
