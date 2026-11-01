#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "opcode.h"

static int failures;

static Expr *lit(int64_t i)
{
	return ast_new_literal(value_int(i), 1);
}

static Expr *name(const char *n)
{
	return ast_new_name(n, (int)strlen(n), 1);
}

static Stmt *assign(const char *n, Expr *e)
{
	return ast_new_assign_stmt(n, (int)strlen(n), e, 1);
}

static Stmt *print(Expr *e)
{
	return ast_new_print_stmt(e, 1);
}

static void check(const char *label, Stmt *body, const uint8_t *want, int n, int nconst)
{
	Program p = { body };
	Chunk c;
	chunk_init(&c);
	if (!compile(&p, &c)) {
		printf("FAIL %s: compile error\n", label);
		failures++;
		return;
	}
	if (c.count != n || memcmp(c.code, want, n) != 0 || c.constants.count != nconst) {
		printf("FAIL %s: got %d bytes, %d consts:", label, c.count, c.constants.count);
		for (int i = 0; i < c.count; i++)
			printf(" %d", c.code[i]);
		printf("\n");
		failures++;
	} else {
		printf("ok   %s\n", label);
	}
	chunk_free(&c);
}

int main(void)
{
	{
		Stmt *s = assign("x", ast_new_binary(TOK_PLUS, lit(1), ast_new_binary(TOK_STAR, lit(2), lit(3), 1), 1));
		s->next = print(name("x"));
		uint8_t w[] = { OP_CONST, 0, OP_CONST, 1, OP_CONST, 2, OP_MUL, OP_ADD,
			OP_SET_GLOBAL, 3, OP_GET_GLOBAL, 4, OP_PRINT, OP_HALT };
		check("assign+arith", s, w, sizeof w, 5);
	}
	{
		Stmt *s = print(ast_new_unary(TOK_NOT, ast_new_unary(TOK_MINUS, lit(1), 1), 1));
		uint8_t w[] = { OP_CONST, 0, OP_NEG, OP_NOT, OP_PRINT, OP_HALT };
		check("unary", s, w, sizeof w, 1);
	}
	{
		Stmt *e = ast_new_expr_stmt(ast_new_binary(TOK_LE, lit(1), lit(2), 1), 1);
		uint8_t w[] = { OP_CONST, 0, OP_CONST, 1, OP_LE, OP_POP, OP_HALT };
		check("expr stmt", e, w, sizeof w, 2);
	}
	{
		Stmt *s = ast_new_if_stmt(ast_new_literal(value_bool(true), 1), print(lit(1)), print(lit(2)), 1);
		uint8_t w[] = { OP_CONST, 0, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
			OP_CONST, 1, OP_PRINT, OP_JUMP, 0, 4, OP_POP,
			OP_CONST, 2, OP_PRINT, OP_HALT };
		check("if/else", s, w, sizeof w, 3);
	}
	{
		Stmt *inner = ast_new_if_stmt(name("b"), print(lit(2)), NULL, 1);
		Stmt *s = ast_new_if_stmt(name("a"), print(lit(1)), inner, 1);
		uint8_t w[] = { OP_GET_GLOBAL, 0, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
			OP_CONST, 1, OP_PRINT, OP_JUMP, 0, 14, OP_POP,
			OP_GET_GLOBAL, 2, OP_JUMP_IF_FALSE, 0, 7, OP_POP,
			OP_CONST, 3, OP_PRINT, OP_JUMP, 0, 1, OP_POP, OP_HALT };
		check("elif", s, w, sizeof w, 4);
	}
	{
		Stmt *body = assign("i", ast_new_binary(TOK_PLUS, name("i"), lit(1), 1));
		Stmt *s = ast_new_while_stmt(ast_new_binary(TOK_LT, name("i"), lit(3), 1), body, 1);
		uint8_t w[] = { OP_GET_GLOBAL, 0, OP_CONST, 1, OP_LT, OP_JUMP_IF_FALSE, 0, 11, OP_POP,
			OP_GET_GLOBAL, 2, OP_CONST, 3, OP_ADD, OP_SET_GLOBAL, 4,
			OP_LOOP, 0, 19, OP_POP, OP_HALT };
		check("while", s, w, sizeof w, 5);
	}
	{
		Stmt *s = print(ast_new_logical(TOK_AND, name("a"), name("b"), 1));
		uint8_t w[] = { OP_GET_GLOBAL, 0, OP_JUMP_IF_FALSE, 0, 3, OP_POP,
			OP_GET_GLOBAL, 1, OP_PRINT, OP_HALT };
		check("and", s, w, sizeof w, 2);
	}
	{
		Stmt *s = print(ast_new_logical(TOK_OR, name("a"), name("b"), 1));
		uint8_t w[] = { OP_GET_GLOBAL, 0, OP_JUMP_IF_FALSE, 0, 3, OP_JUMP, 0, 3,
			OP_POP, OP_GET_GLOBAL, 1, OP_PRINT, OP_HALT };
		check("or", s, w, sizeof w, 2);
	}
	{
		Stmt *head = NULL, *tail = NULL;
		for (int i = 0; i < 257; i++) {
			Stmt *s = print(lit(i));
			if (tail)
				tail->next = s;
			else
				head = s;
			tail = s;
		}
		Program p = { head };
		Chunk c;
		chunk_init(&c);
		if (compile(&p, &c)) {
			printf("FAIL too many constants: compiled\n");
			failures++;
		} else {
			printf("ok   too many constants\n");
		}
		chunk_free(&c);
	}
	return failures ? 1 : 0;
}
