#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lexer.h"

static int failures;

static void expect(const char *src, const TokenType *want, int n)
{
	Lexer *lx = lexer_new(src);
	int i;

	for (i = 0; i < n; i++) {
		Token t = lexer_next(lx);
		if (t.type != want[i]) {
			fprintf(stderr, "FAIL [%s] token %d: got %d want %d\n", src, i, t.type, want[i]);
			failures++;
			break;
		}
	}
	if (i == n && want[n - 1] == TOK_EOF && lexer_next(lx).type != TOK_EOF) {
		fprintf(stderr, "FAIL [%s]: EOF not sticky\n", src);
		failures++;
	}
	lexer_free(lx);
}

#define EXPECT(src, ...) do { \
	TokenType w[] = {__VA_ARGS__}; \
	expect(src, w, (int)(sizeof w / sizeof w[0])); \
} while (0)

int main(void)
{
	const char *sample =
		"# header\n"
		"x = 1\n"
		"\n"
		"while x < 10:  # loop\n"
		"    if x == 1:\n"
		"        print(\"a\\\"b\")\n"
		"    elif x != 2:\n"
		"\n"
		"        print(1.5)\n"
		"    else:\n"
		"        print(None)\n"
		"    x = x + 1\n"
		"print(x)";

	EXPECT(sample,
		TOK_NAME, TOK_EQ, TOK_INT, TOK_NEWLINE,
		TOK_WHILE, TOK_NAME, TOK_LT, TOK_INT, TOK_COLON, TOK_NEWLINE,
		TOK_INDENT, TOK_IF, TOK_NAME, TOK_EQEQ, TOK_INT, TOK_COLON, TOK_NEWLINE,
		TOK_INDENT, TOK_PRINT, TOK_LPAREN, TOK_STRING, TOK_RPAREN, TOK_NEWLINE,
		TOK_DEDENT, TOK_ELIF, TOK_NAME, TOK_NE, TOK_INT, TOK_COLON, TOK_NEWLINE,
		TOK_INDENT, TOK_PRINT, TOK_LPAREN, TOK_FLOAT, TOK_RPAREN, TOK_NEWLINE,
		TOK_DEDENT, TOK_ELSE, TOK_COLON, TOK_NEWLINE,
		TOK_INDENT, TOK_PRINT, TOK_LPAREN, TOK_NONE, TOK_RPAREN, TOK_NEWLINE,
		TOK_DEDENT, TOK_NAME, TOK_EQ, TOK_NAME, TOK_PLUS, TOK_INT, TOK_NEWLINE,
		TOK_DEDENT, TOK_PRINT, TOK_LPAREN, TOK_NAME, TOK_RPAREN, TOK_NEWLINE,
		TOK_EOF);

	EXPECT("if True:\n    x = 1\n",
		TOK_IF, TOK_TRUE, TOK_COLON, TOK_NEWLINE, TOK_INDENT,
		TOK_NAME, TOK_EQ, TOK_INT, TOK_NEWLINE, TOK_DEDENT, TOK_EOF);
	EXPECT("if a:\n  if b:\n    c\n",
		TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE, TOK_INDENT,
		TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE, TOK_INDENT,
		TOK_NAME, TOK_NEWLINE, TOK_DEDENT, TOK_DEDENT, TOK_EOF);
	EXPECT("", TOK_EOF);
	EXPECT("\n# only\n\n", TOK_EOF);
	EXPECT("a <= b >= c and not d or e % 2 / 3 * 4 - 5, False",
		TOK_NAME, TOK_LE, TOK_NAME, TOK_GE, TOK_NAME, TOK_AND, TOK_NOT, TOK_NAME,
		TOK_OR, TOK_NAME, TOK_PERCENT, TOK_INT, TOK_SLASH, TOK_INT, TOK_STAR,
		TOK_INT, TOK_MINUS, TOK_INT, TOK_COMMA, TOK_FALSE, TOK_NEWLINE, TOK_EOF);
	EXPECT("print(1,\n  2)\n", TOK_PRINT, TOK_LPAREN, TOK_INT, TOK_COMMA, TOK_INT,
		TOK_RPAREN, TOK_NEWLINE, TOK_EOF);

	EXPECT("if a:\n        x\n    y\n",
		TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE, TOK_INDENT,
		TOK_NAME, TOK_NEWLINE, TOK_ERROR);
	EXPECT("if a:\n\tx\n", TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE, TOK_ERROR);
	EXPECT("x = \"abc\n", TOK_NAME, TOK_EQ, TOK_ERROR);
	EXPECT("x = 'abc", TOK_NAME, TOK_EQ, TOK_ERROR);
	EXPECT("x = \"abc\\", TOK_NAME, TOK_EQ, TOK_ERROR);

	{
		Lexer *lx = lexer_new("s = 'it\\'s' 12 3.25 foo");
		Token t;
		lexer_next(lx);
		lexer_next(lx);
		t = lexer_next(lx);
		if (t.type != TOK_STRING || t.length != 7 || memcmp(t.start, "'it\\'s'", 7)) {
			fprintf(stderr, "FAIL string text\n");
			failures++;
		}
		t = lexer_next(lx);
		if (t.type != TOK_INT || t.length != 2 || t.line != 1) {
			fprintf(stderr, "FAIL int\n");
			failures++;
		}
		t = lexer_next(lx);
		if (t.type != TOK_FLOAT || t.length != 4) {
			fprintf(stderr, "FAIL float\n");
			failures++;
		}
		lexer_free(lx);
	}

	if (failures) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	puts("lexer ok");
	return 0;
}
