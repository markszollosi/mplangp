#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "chunk.h"
#include "compiler.h"
#include "vm.h"

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "pyclone: cannot open '%s'\n", path);
		return NULL;
	}
	size_t cap = 4096, len = 0;
	char *buf = malloc(cap);
	while (buf) {
		len += fread(buf + len, 1, cap - len - 1, f);
		if (feof(f) || ferror(f))
			break;
		char *p = realloc(buf, cap * 2);
		if (!p) {
			free(buf);
			buf = NULL;
			break;
		}
		buf = p;
		cap *= 2;
	}
	int bad = ferror(f);
	fclose(f);
	if (!buf || bad) {
		fprintf(stderr, "pyclone: cannot read '%s'\n", path);
		free(buf);
		return NULL;
	}
	buf[len] = '\0';
	return buf;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: pyclone <file.py>\n");
		return 1;
	}
	char *source = read_file(argv[1]);
	if (!source)
		return 1;

	Lexer *lx = lexer_new(source);
	Program *prog = parse(lx);
	lexer_free(lx);
	if (!prog) {
		free(source);
		return 65;
	}

	Chunk chunk;
	chunk_init(&chunk);
	bool ok = compile(prog, &chunk);
	program_free(prog);
	free(source);
	if (!ok) {
		chunk_free(&chunk);
		return 65;
	}

	vm_init();
	InterpretResult r = vm_run(&chunk);
	vm_free();
	chunk_free(&chunk);
	return r == INTERPRET_OK ? 0 : 70;
}
