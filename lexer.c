#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

struct Lexer {
	const char *cur;
	int line;
	int at_line_start;
	int has_tokens;
	int paren;
	int pending;
	int *stack;
	int depth;
	int cap;
};

static const struct {
	const char *name;
	TokenType type;
} keywords[] = {
	{"True", TOK_TRUE}, {"False", TOK_FALSE}, {"None", TOK_NONE},
	{"if", TOK_IF}, {"elif", TOK_ELIF}, {"else", TOK_ELSE},
	{"while", TOK_WHILE}, {"print", TOK_PRINT},
	{"and", TOK_AND}, {"or", TOK_OR}, {"not", TOK_NOT},
};

Lexer *lexer_new(const char *source)
{
	Lexer *lx = malloc(sizeof *lx);
	if (!lx) {
		return NULL;
	}
	lx->cap = 16;
	lx->stack = malloc(sizeof(int) * lx->cap);
	if (!lx->stack) {
		free(lx);
		return NULL;
	}
	lx->stack[0] = 0;
	lx->depth = 1;
	lx->cur = source;
	lx->line = 1;
	lx->at_line_start = 1;
	lx->has_tokens = 0;
	lx->paren = 0;
	lx->pending = 0;
	return lx;
}

void lexer_free(Lexer *lx)
{
	if (lx) {
		free(lx->stack);
		free(lx);
	}
}

static Token make(Lexer *lx, TokenType type, const char *start, int length)
{
	Token t;
	t.type = type;
	t.start = start;
	t.length = length;
	t.line = lx->line;
	if (type != TOK_INDENT && type != TOK_DEDENT && type != TOK_NEWLINE && type != TOK_EOF) {
		lx->has_tokens = 1;
	}
	return t;
}

static Token error(Lexer *lx, const char *msg)
{
	return make(lx, TOK_ERROR, msg, (int)strlen(msg));
}

static int push_indent(Lexer *lx, int width)
{
	if (lx->depth == lx->cap) {
		int *p = realloc(lx->stack, sizeof(int) * lx->cap * 2);
		if (!p) {
			return 0;
		}
		lx->stack = p;
		lx->cap *= 2;
	}
	lx->stack[lx->depth++] = width;
	return 1;
}

static Token indentation(Lexer *lx, int *done)
{
	const char *p = lx->cur;
	Token none = {TOK_EOF, NULL, 0, 0};
	int width = 0;

	*done = 0;
	for (;;) {
		while (*p == ' ') {
			width++;
			p++;
		}
		if (*p == '\t') {
			lx->cur = p + 1;
			lx->at_line_start = 0;
			*done = 1;
			return error(lx, "tab in indentation");
		}
		if (*p == '#') {
			while (*p && *p != '\n') {
				p++;
			}
		}
		if (*p == '\r') {
			p++;
		}
		if (*p == '\n') {
			p++;
			lx->line++;
			width = 0;
			continue;
		}
		break;
	}
	lx->cur = p;
	lx->at_line_start = 0;
	if (*p == '\0') {
		return none;
	}
	if (width > lx->stack[lx->depth - 1]) {
		*done = 1;
		if (!push_indent(lx, width)) {
			return error(lx, "out of memory");
		}
		return make(lx, TOK_INDENT, p, 0);
	}
	if (width < lx->stack[lx->depth - 1]) {
		int n = 0;
		while (lx->depth > 1 && lx->stack[lx->depth - 1] > width) {
			lx->depth--;
			n++;
		}
		*done = 1;
		if (lx->stack[lx->depth - 1] != width) {
			return error(lx, "inconsistent dedent");
		}
		lx->pending = n - 1;
		return make(lx, TOK_DEDENT, p, 0);
	}
	return none;
}

static Token scan_string(Lexer *lx)
{
	const char *start = lx->cur;
	char q = *lx->cur++;

	while (*lx->cur && *lx->cur != q && *lx->cur != '\n') {
		if (*lx->cur == '\\' && lx->cur[1] && lx->cur[1] != '\n') {
			lx->cur++;
		}
		lx->cur++;
	}
	if (*lx->cur != q) {
		return error(lx, "unterminated string");
	}
	lx->cur++;
	return make(lx, TOK_STRING, start, (int)(lx->cur - start));
}

static Token scan_number(Lexer *lx)
{
	const char *start = lx->cur;
	TokenType type = TOK_INT;

	while (isdigit((unsigned char)*lx->cur)) {
		lx->cur++;
	}
	if (*lx->cur == '.' && isdigit((unsigned char)lx->cur[1])) {
		type = TOK_FLOAT;
		lx->cur++;
		while (isdigit((unsigned char)*lx->cur)) {
			lx->cur++;
		}
	}
	return make(lx, type, start, (int)(lx->cur - start));
}

static Token scan_name(Lexer *lx)
{
	const char *start = lx->cur;
	size_t i, len;

	while (isalnum((unsigned char)*lx->cur) || *lx->cur == '_') {
		lx->cur++;
	}
	len = (size_t)(lx->cur - start);
	for (i = 0; i < sizeof keywords / sizeof keywords[0]; i++) {
		if (strlen(keywords[i].name) == len && !memcmp(keywords[i].name, start, len)) {
			return make(lx, keywords[i].type, start, (int)len);
		}
	}
	return make(lx, TOK_NAME, start, (int)len);
}

static Token op(Lexer *lx, TokenType one, TokenType two)
{
	const char *start = lx->cur++;
	if (two != TOK_ERROR && *lx->cur == '=') {
		lx->cur++;
		return make(lx, two, start, 2);
	}
	return make(lx, one, start, 1);
}

Token lexer_next(Lexer *lx)
{
	for (;;) {
		char c;

		if (lx->pending > 0) {
			lx->pending--;
			return make(lx, TOK_DEDENT, lx->cur, 0);
		}
		if (lx->at_line_start && lx->paren == 0) {
			int done;
			Token t = indentation(lx, &done);
			if (done) {
				return t;
			}
		}
		while (*lx->cur == ' ' || *lx->cur == '\t' || *lx->cur == '\r') {
			lx->cur++;
		}
		c = *lx->cur;
		if (c == '#') {
			while (*lx->cur && *lx->cur != '\n') {
				lx->cur++;
			}
			continue;
		}
		if (c == '\0') {
			if (lx->has_tokens) {
				lx->has_tokens = 0;
				return make(lx, TOK_NEWLINE, lx->cur, 0);
			}
			if (lx->depth > 1) {
				lx->depth--;
				return make(lx, TOK_DEDENT, lx->cur, 0);
			}
			return make(lx, TOK_EOF, lx->cur, 0);
		}
		if (c == '\n') {
			Token t = make(lx, TOK_NEWLINE, lx->cur, 1);
			lx->cur++;
			lx->line++;
			if (lx->paren > 0 || !lx->has_tokens) {
				continue;
			}
			lx->has_tokens = 0;
			lx->at_line_start = 1;
			return t;
		}
		if (c == '"' || c == '\'') {
			return scan_string(lx);
		}
		if (isdigit((unsigned char)c)) {
			return scan_number(lx);
		}
		if (isalpha((unsigned char)c) || c == '_') {
			return scan_name(lx);
		}
		switch (c) {
		case '+': return op(lx, TOK_PLUS, TOK_ERROR);
		case '-': return op(lx, TOK_MINUS, TOK_ERROR);
		case '*': return op(lx, TOK_STAR, TOK_ERROR);
		case '/': return op(lx, TOK_SLASH, TOK_ERROR);
		case '%': return op(lx, TOK_PERCENT, TOK_ERROR);
		case '=': return op(lx, TOK_EQ, TOK_EQEQ);
		case '<': return op(lx, TOK_LT, TOK_LE);
		case '>': return op(lx, TOK_GT, TOK_GE);
		case ':': return op(lx, TOK_COLON, TOK_ERROR);
		case ',': return op(lx, TOK_COMMA, TOK_ERROR);
		case '(':
			lx->paren++;
			return op(lx, TOK_LPAREN, TOK_ERROR);
		case ')':
			if (lx->paren > 0) {
				lx->paren--;
			}
			return op(lx, TOK_RPAREN, TOK_ERROR);
		case '!':
			if (lx->cur[1] == '=') {
				const char *s = lx->cur;
				lx->cur += 2;
				return make(lx, TOK_NE, s, 2);
			}
			lx->cur++;
			return error(lx, "unexpected character '!'");
		default:
			lx->cur++;
			return error(lx, "unexpected character");
		}
	}
}
