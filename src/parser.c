#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "util.h"

typedef struct {
	Lexer *lx;
	Token cur;
	bool failed;
	bool in_def;
} Parser;

static Expr *parse_expr(Parser *p);
static Stmt *parse_stmt(Parser *p);

static void advance(Parser *p)
{
	p->cur = lexer_next(p->lx);
}

static void error_at(Parser *p, const char *msg)
{
	if (p->failed) {
		return;
	}
	p->failed = true;
	if (p->cur.type == TOK_ERROR) {
		fprintf(stderr, "line %d: %.*s\n", p->cur.line, p->cur.length, p->cur.start);
	} else {
		fprintf(stderr, "line %d: %s\n", p->cur.line, msg);
	}
}

static bool expect(Parser *p, TokenType type, const char *msg)
{
	if (p->cur.type != type) {
		error_at(p, msg);
		return false;
	}
	advance(p);
	return true;
}

static Expr *parse_string(Parser *p)
{
	const char *s = p->cur.start + 1;
	int n = p->cur.length - 2;
	char *buf = xmalloc(n + 1);
	int len = 0;
	for (int i = 0; i < n; i++) {
		if (s[i] == '\\' && i + 1 < n) {
			i++;
			switch (s[i]) {
			case 'n': buf[len++] = '\n'; break;
			case '"': buf[len++] = '"'; break;
			case '\\': buf[len++] = '\\'; break;
			default:
				buf[len++] = '\\';
				buf[len++] = s[i];
				break;
			}
		} else {
			buf[len++] = s[i];
		}
	}
	Expr *e = ast_new_literal(value_string(buf, len), p->cur.line);
	free(buf);
	advance(p);
	return e;
}

static Expr *parse_primary(Parser *p)
{
	Token t = p->cur;
	Expr *e;
	char num[64];
	switch (t.type) {
	case TOK_INT:
	case TOK_FLOAT:
		if (t.length >= (int)sizeof num) {
			error_at(p, "number too long");
			return NULL;
		}
		memcpy(num, t.start, t.length);
		num[t.length] = '\0';
		errno = 0;
		if (t.type == TOK_INT) {
			long long v = strtoll(num, NULL, 10);
			if (errno == ERANGE) {
				error_at(p, "integer literal too large");
				return NULL;
			}
			e = ast_new_literal(value_int(v), t.line);
		} else {
			e = ast_new_literal(value_float(strtod(num, NULL)), t.line);
		}
		advance(p);
		return e;
	case TOK_STRING:
		return parse_string(p);
	case TOK_TRUE:
		advance(p);
		return ast_new_literal(value_bool(true), t.line);
	case TOK_FALSE:
		advance(p);
		return ast_new_literal(value_bool(false), t.line);
	case TOK_NONE:
		advance(p);
		return ast_new_literal(value_none(), t.line);
	case TOK_NAME:
		advance(p);
		return ast_new_name(t.start, t.length, t.line);
	case TOK_LPAREN:
		advance(p);
		e = parse_expr(p);
		if (!e || !expect(p, TOK_RPAREN, "expected ')'")) {
			return NULL;
		}
		return e;
	default:
		error_at(p, "invalid syntax");
		return NULL;
	}
}

static Expr *parse_postfix(Parser *p)
{
	Expr *e = parse_primary(p);
	while (e && (p->cur.type == TOK_LPAREN || p->cur.type == TOK_DOT)) {
		int line = p->cur.line;
		if (p->cur.type == TOK_DOT) {
			advance(p);
			Token name = p->cur;
			if (!expect(p, TOK_NAME, "expected a name after '.'")) {
				expr_free(e);
				return NULL;
			}
			e = ast_new_attr(e, name.start, name.length, line);
			continue;
		}
		advance(p);
		Expr **args = NULL;
		int n = 0, cap = 0;
		bool ok = true;
		if (p->cur.type != TOK_RPAREN) {
			for (;;) {
				Expr *a = parse_expr(p);
				if (!a) {
					ok = false;
					break;
				}
				if (n == cap) {
					cap = cap ? cap * 2 : 4;
					args = xrealloc(args, (size_t)cap * sizeof *args);
				}
				args[n++] = a;
				if (p->cur.type != TOK_COMMA)
					break;
				advance(p);
			}
		}
		if (!ok || !expect(p, TOK_RPAREN, "expected ')'")) {
			for (int i = 0; i < n; i++)
				expr_free(args[i]);
			free(args);
			expr_free(e);
			return NULL;
		}
		e = ast_new_call(e, args, n, line);
	}
	return e;
}

static Expr *parse_unary(Parser *p)
{
	if (p->cur.type == TOK_MINUS) {
		int line = p->cur.line;
		advance(p);
		Expr *r = parse_unary(p);
		return r ? ast_new_unary(TOK_MINUS, r, line) : NULL;
	}
	return parse_postfix(p);
}

static Expr *parse_factor(Parser *p)
{
	Expr *e = parse_unary(p);
	while (e && (p->cur.type == TOK_STAR || p->cur.type == TOK_SLASH || p->cur.type == TOK_PERCENT)) {
		Token op = p->cur;
		advance(p);
		Expr *r = parse_unary(p);
		e = r ? ast_new_binary(op.type, e, r, op.line) : NULL;
	}
	return e;
}

static Expr *parse_term(Parser *p)
{
	Expr *e = parse_factor(p);
	while (e && (p->cur.type == TOK_PLUS || p->cur.type == TOK_MINUS)) {
		Token op = p->cur;
		advance(p);
		Expr *r = parse_factor(p);
		e = r ? ast_new_binary(op.type, e, r, op.line) : NULL;
	}
	return e;
}

static bool is_comparison(TokenType t)
{
	return t == TOK_EQEQ || t == TOK_NE || t == TOK_LT || t == TOK_LE || t == TOK_GT || t == TOK_GE;
}

static Expr *parse_comparison(Parser *p)
{
	Expr *e = parse_term(p);
	while (e && is_comparison(p->cur.type)) {
		Token op = p->cur;
		advance(p);
		Expr *r = parse_term(p);
		e = r ? ast_new_binary(op.type, e, r, op.line) : NULL;
	}
	return e;
}

static Expr *parse_not(Parser *p)
{
	if (p->cur.type == TOK_NOT) {
		int line = p->cur.line;
		advance(p);
		Expr *r = parse_not(p);
		return r ? ast_new_unary(TOK_NOT, r, line) : NULL;
	}
	return parse_comparison(p);
}

static Expr *parse_and(Parser *p)
{
	Expr *e = parse_not(p);
	while (e && p->cur.type == TOK_AND) {
		int line = p->cur.line;
		advance(p);
		Expr *r = parse_not(p);
		e = r ? ast_new_logical(TOK_AND, e, r, line) : NULL;
	}
	return e;
}

static Expr *parse_expr(Parser *p)
{
	Expr *e = parse_and(p);
	while (e && p->cur.type == TOK_OR) {
		int line = p->cur.line;
		advance(p);
		Expr *r = parse_and(p);
		e = r ? ast_new_logical(TOK_OR, e, r, line) : NULL;
	}
	return e;
}

static void free_stmts(Stmt *s)
{
	Program *p = xmalloc(sizeof *p);
	p->body = s;
	program_free(p);
}

static Stmt *parse_block(Parser *p)
{
	if (!expect(p, TOK_NEWLINE, "expected newline after ':'") ||
	    !expect(p, TOK_INDENT, "expected an indented block")) {
		return NULL;
	}
	Stmt *head = NULL, **tail = &head;
	while (p->cur.type != TOK_DEDENT && p->cur.type != TOK_EOF) {
		Stmt *s = parse_stmt(p);
		if (!s) {
			free_stmts(head);
			return NULL;
		}
		*tail = s;
		tail = &s->next;
	}
	if (!expect(p, TOK_DEDENT, "unexpected end of input in block")) {
		free_stmts(head);
		return NULL;
	}
	return head;
}

static Stmt *parse_if(Parser *p)
{
	int line = p->cur.line;
	advance(p);
	Expr *cond = parse_expr(p);
	if (!cond || !expect(p, TOK_COLON, "expected ':'")) {
		return NULL;
	}
	Stmt *then_branch = parse_block(p);
	if (!then_branch) {
		return NULL;
	}
	Stmt *else_branch = NULL;
	if (p->cur.type == TOK_ELIF) {
		else_branch = parse_if(p);
		if (!else_branch) {
			free_stmts(then_branch);
			return NULL;
		}
	} else if (p->cur.type == TOK_ELSE) {
		advance(p);
		if (!expect(p, TOK_COLON, "expected ':'") || !(else_branch = parse_block(p))) {
			free_stmts(then_branch);
			return NULL;
		}
	}
	return ast_new_if_stmt(cond, then_branch, else_branch, line);
}

static Stmt *parse_def(Parser *p)
{
	int line = p->cur.line;
	if (p->in_def) {
		error_at(p, "nested def is not supported");
		return NULL;
	}
	advance(p);
	Token name = p->cur;
	if (!expect(p, TOK_NAME, "expected a function name") ||
	    !expect(p, TOK_LPAREN, "expected '(' after function name")) {
		return NULL;
	}
	Expr **params = NULL;
	int n = 0, cap = 0;
	bool ok = true;
	if (p->cur.type != TOK_RPAREN) {
		for (;;) {
			Token t = p->cur;
			if (!expect(p, TOK_NAME, "expected a parameter name")) {
				ok = false;
				break;
			}
			if (n == cap) {
				cap = cap ? cap * 2 : 4;
				params = xrealloc(params, (size_t)cap * sizeof *params);
			}
			params[n++] = ast_new_name(t.start, t.length, t.line);
			if (p->cur.type != TOK_COMMA)
				break;
			advance(p);
		}
	}
	Stmt *body = NULL;
	if (ok && expect(p, TOK_RPAREN, "expected ')'") && expect(p, TOK_COLON, "expected ':'")) {
		p->in_def = true;
		body = parse_block(p);
		p->in_def = false;
	}
	if (!body) {
		for (int i = 0; i < n; i++)
			expr_free(params[i]);
		free(params);
		return NULL;
	}
	return ast_new_def_stmt(name.start, name.length, params, n, body, line);
}

static Stmt *parse_stmt(Parser *p)
{
	int line = p->cur.line;
	Expr *e;
	Stmt *body;
	switch (p->cur.type) {
	case TOK_IF:
		return parse_if(p);
	case TOK_DEF:
		return parse_def(p);
	case TOK_RETURN:
		if (!p->in_def) {
			error_at(p, "'return' outside function");
			return NULL;
		}
		advance(p);
		e = NULL;
		if (p->cur.type != TOK_NEWLINE && !(e = parse_expr(p))) {
			return NULL;
		}
		if (!expect(p, TOK_NEWLINE, "expected newline")) {
			expr_free(e);
			return NULL;
		}
		return ast_new_return_stmt(e, line);
	case TOK_BREAK:
	case TOK_CONTINUE: {
		TokenType kind = p->cur.type;
		advance(p);
		if (!expect(p, TOK_NEWLINE, "expected newline")) {
			return NULL;
		}
		return kind == TOK_BREAK ? ast_new_break_stmt(line) : ast_new_continue_stmt(line);
	}
	case TOK_IMPORT: {
		advance(p);
		Token name = p->cur;
		if (!expect(p, TOK_NAME, "expected a module name") ||
		    !expect(p, TOK_NEWLINE, "expected newline")) {
			return NULL;
		}
		return ast_new_import_stmt(name.start, name.length, line);
	}
	case TOK_WHILE:
		advance(p);
		e = parse_expr(p);
		if (!e || !expect(p, TOK_COLON, "expected ':'")) {
			return NULL;
		}
		body = parse_block(p);
		return body ? ast_new_while_stmt(e, body, line) : NULL;
	case TOK_PRINT:
		advance(p);
		if (!expect(p, TOK_LPAREN, "expected '(' after print")) {
			return NULL;
		}
		e = parse_expr(p);
		if (!e || !expect(p, TOK_RPAREN, "expected ')'") || !expect(p, TOK_NEWLINE, "expected newline")) {
			return NULL;
		}
		return ast_new_print_stmt(e, line);
	default:
		e = parse_expr(p);
		if (!e) {
			return NULL;
		}
		if (p->cur.type == TOK_EQ) {
			if (e->type != EXPR_NAME) {
				error_at(p, "cannot assign to expression");
				return NULL;
			}
			advance(p);
			Expr *v = parse_expr(p);
			if (!v || !expect(p, TOK_NEWLINE, "expected newline")) {
				return NULL;
			}
			Stmt *a = ast_new_assign_stmt(e->as.name.name, e->as.name.length, v, line);
			free(e);
			return a;
		}
		if (!expect(p, TOK_NEWLINE, "expected newline")) {
			return NULL;
		}
		return ast_new_expr_stmt(e, line);
	}
}

Program *parse(Lexer *lx)
{
	Parser p = { lx, { TOK_EOF, NULL, 0, 0 }, false, false };
	advance(&p);
	Program *prog = xmalloc(sizeof *prog);
	prog->body = NULL;
	Stmt **tail = &prog->body;
	while (p.cur.type != TOK_EOF) {
		Stmt *s = parse_stmt(&p);
		if (!s) {
			program_free(prog);
			return NULL;
		}
		*tail = s;
		tail = &s->next;
	}
	return prog;
}
