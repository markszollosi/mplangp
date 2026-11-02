#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static int failures;

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static Program *parse_src(const char *src)
{
	Lexer *lx = lexer_new(src);
	Program *p = parse(lx);
	lexer_free(lx);
	return p;
}

static bool is_int(Expr *e, int64_t v)
{
	return e->type == EXPR_LITERAL && e->as.literal.type == VAL_INT && e->as.literal.as.i == v;
}

static bool is_name(Expr *e, const char *n)
{
	return e->type == EXPR_NAME && e->as.name.length == (int)strlen(n) && !strncmp(e->as.name.name, n, e->as.name.length);
}

static void test_precedence(void)
{
	Program *p = parse_src("1 + 2 * 3 - -4\n");
	CHECK(p && p->body && p->body->type == STMT_EXPR && !p->body->next);
	Expr *e = p->body->as.expr;
	CHECK(e->type == EXPR_BINARY && e->as.binary.op == TOK_MINUS);
	CHECK(e->as.binary.right->type == EXPR_UNARY && is_int(e->as.binary.right->as.unary.right, 4));
	Expr *sum = e->as.binary.left;
	CHECK(sum->type == EXPR_BINARY && sum->as.binary.op == TOK_PLUS && is_int(sum->as.binary.left, 1));
	CHECK(sum->as.binary.right->as.binary.op == TOK_STAR);

	p = parse_src("(1 + 2) * 3\n");
	e = p->body->as.expr;
	CHECK(e->as.binary.op == TOK_STAR && e->as.binary.left->as.binary.op == TOK_PLUS);

	p = parse_src("a or b and not c == 1\n");
	e = p->body->as.expr;
	CHECK(e->type == EXPR_LOGICAL && e->as.logical.op == TOK_OR && is_name(e->as.logical.left, "a"));
	e = e->as.logical.right;
	CHECK(e->type == EXPR_LOGICAL && e->as.logical.op == TOK_AND);
	e = e->as.logical.right;
	CHECK(e->type == EXPR_UNARY && e->as.unary.op == TOK_NOT);
	CHECK(e->as.unary.right->type == EXPR_BINARY && e->as.unary.right->as.binary.op == TOK_EQEQ);

	p = parse_src("10 - 3 - 2\n");
	e = p->body->as.expr;
	CHECK(is_int(e->as.binary.right, 2) && e->as.binary.left->type == EXPR_BINARY);
}

static void test_literals_assign(void)
{
	Program *p = parse_src("x = 1.5\ny = True\nz = None\ns = \"a\\nb\\\"c\\\\\"\nt = 'hi'\n");
	Stmt *s = p->body;
	CHECK(s->type == STMT_ASSIGN && s->as.assign.length == 1 && s->as.assign.name[0] == 'x');
	CHECK(s->as.assign.value->as.literal.type == VAL_FLOAT && s->as.assign.value->as.literal.as.f == 1.5);
	s = s->next;
	CHECK(s->as.assign.value->as.literal.type == VAL_BOOL && s->as.assign.value->as.literal.as.b);
	s = s->next;
	CHECK(s->as.assign.value->as.literal.type == VAL_NONE);
	s = s->next;
	CHECK(s->as.assign.value->as.literal.type == VAL_STRING);
	CHECK(!strcmp(s->as.assign.value->as.literal.as.s, "a\nb\"c\\"));
	s = s->next;
	CHECK(!strcmp(s->as.assign.value->as.literal.as.s, "hi"));
	CHECK(!s->next);
}

static void test_control(void)
{
	Program *p = parse_src(
		"i = 0\n"
		"while i < 3:\n"
		"    j = 0\n"
		"    while j < i:\n"
		"        print(j)\n"
		"        j = j + 1\n"
		"    i = i + 1\n"
		"if i == 1:\n"
		"    print(1)\n"
		"elif i == 3:\n"
		"    print(3)\n"
		"else:\n"
		"    print(\"x\")\n"
		"    print(2)\n"
		"print(i)\n");
	CHECK(p);
	Stmt *w = p->body->next;
	CHECK(w->type == STMT_WHILE && w->as.while_stmt.cond->type == EXPR_BINARY);
	Stmt *b = w->as.while_stmt.body;
	CHECK(b->type == STMT_ASSIGN && b->next->type == STMT_WHILE && b->next->next->type == STMT_ASSIGN && !b->next->next->next);
	Stmt *inner = b->next->as.while_stmt.body;
	CHECK(inner->type == STMT_PRINT && inner->next->type == STMT_ASSIGN && !inner->next->next);
	Stmt *i = w->next;
	CHECK(i->type == STMT_IF && i->as.if_stmt.then_branch->type == STMT_PRINT);
	Stmt *el = i->as.if_stmt.else_branch;
	CHECK(el && el->type == STMT_IF && el->as.if_stmt.else_branch);
	Stmt *ee = el->as.if_stmt.else_branch;
	CHECK(ee->type == STMT_PRINT && ee->next && !ee->next->next);
	CHECK(i->next->type == STMT_PRINT && !i->next->next);

	p = parse_src("if True:\n    x = 1\nprint(x)\n");
	CHECK(p && p->body->type == STMT_IF && !p->body->as.if_stmt.else_branch && p->body->next->type == STMT_PRINT);

	p = parse_src("");
	CHECK(p && !p->body);
}

static void test_errors(void)
{
	const char *bad[] = {
		"1 +\n", "x = \n", "print 1\n", "print(1\n", "if x\n    y\n", "if x:\ny = 1\n",
		"(1 + 2\n", "1 = 2\n", "x = 1 2\n", "else:\n    x\n", "x = $\n", "while:\n    x\n", "    x = 1\n",
	};
	for (size_t k = 0; k < sizeof bad / sizeof *bad; k++) {
		fprintf(stderr, "[expected error] ");
		Program *p = parse_src(bad[k]);
		if (p) {
			fprintf(stderr, "FAIL: accepted %s", bad[k]);
			failures++;
		}
	}
}

static void test_depth(void)
{
	enum { N = 300 };
	char *src = malloc(2 * N + 16);
	int n = 0;
	src[n++] = 'x';
	src[n++] = '=';
	for (int i = 0; i < N; i++)
		src[n++] = '(';
	src[n++] = '1';
	for (int i = 0; i < N; i++)
		src[n++] = ')';
	src[n++] = '\n';
	src[n] = '\0';
	CHECK(parse_src(src) == NULL);
	free(src);
	Program *p = parse_src("x = ((((1))))\n");
	CHECK(p != NULL);
	program_free(p);
	CHECK(parse_src("f(1, [2, {3: (4,\n") == NULL);
	CHECK(parse_src("def f(a, b:\n") == NULL);
}

int main(void)
{
	test_depth();
	test_precedence();
	test_literals_assign();
	test_control();
	test_errors();
	if (failures) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	puts("ok");
	return 0;
}
