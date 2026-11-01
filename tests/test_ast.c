#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../ast.h"
#include "../util.h"
#include "../value.h"


int main(void)
{
	const char *src = "x = 0\nwhile x < 3:\n if x == 1: print(\"one\")\n";
	Stmt *print_one = ast_new_print_stmt(ast_new_literal(value_string("one", 3), 3), 3);
	Stmt *print_x = ast_new_print_stmt(ast_new_name(src, 1, 5), 5);
	Stmt *inner = ast_new_if_stmt(
		ast_new_binary(TOK_EQEQ, ast_new_name(src, 1, 3), ast_new_literal(value_int(1), 3), 3),
		print_one,
		ast_new_if_stmt(
			ast_new_logical(TOK_OR, ast_new_unary(TOK_NOT, ast_new_name(src, 1, 4), 4),
				ast_new_literal(value_int(2), 4), 4),
			print_x, NULL, 4),
		3);
	Stmt *incr = ast_new_assign_stmt(src, 1, ast_new_binary(TOK_PLUS, ast_new_name(src, 1, 6),
		ast_new_literal(value_int(1), 6), 6), 6);
	inner->next = incr;
	Stmt *loop = ast_new_while_stmt(
		ast_new_binary(TOK_LT, ast_new_name(src, 1, 2), ast_new_literal(value_int(3), 2), 2),
		inner, 2);
	Stmt *init = ast_new_assign_stmt(src, 1, ast_new_literal(value_int(0), 1), 1);
	init->next = loop;
	Program *p = xmalloc(sizeof *p);
	p->body = init;

	assert(p->body->type == STMT_ASSIGN && p->body->line == 1);
	assert(p->body->as.assign.value->type == EXPR_LITERAL);
	assert(p->body->as.assign.value->as.literal.as.i == 0);
	assert(p->body->next == loop && loop->next == NULL);
	assert(loop->type == STMT_WHILE);
	assert(loop->as.while_stmt.cond->type == EXPR_BINARY);
	assert(loop->as.while_stmt.cond->as.binary.op == TOK_LT);
	assert(loop->as.while_stmt.body == inner && inner->next == incr);
	assert(inner->type == STMT_IF);
	assert(inner->as.if_stmt.then_branch == print_one);
	assert(print_one->as.expr->as.literal.type == VAL_STRING);
	assert(strcmp(print_one->as.expr->as.literal.as.s, "one") == 0);
	Stmt *elif = inner->as.if_stmt.else_branch;
	assert(elif->type == STMT_IF && elif->as.if_stmt.else_branch == NULL);
	assert(elif->as.if_stmt.cond->type == EXPR_LOGICAL);
	assert(elif->as.if_stmt.cond->as.logical.op == TOK_OR);
	assert(elif->as.if_stmt.cond->as.logical.left->type == EXPR_UNARY);
	assert(elif->as.if_stmt.then_branch->type == STMT_PRINT);

	program_free(p);
	program_free(NULL);
	puts("test_ast ok");
	return 0;
}
