#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "chunk.h"
#include "compiler.h"
#include <string.h>
#include "module.h"
#include "builtin.h"
#include "vm.h"

static void add_module_paths(const char *script)
{
	const char *slash = strrchr(script, '/');
	if (slash) {
		size_t n = (size_t)(slash - script);
		char *dir = malloc(n + 9);
		if (dir) {
			memcpy(dir, script, n);
			strcpy(dir + n, "/modules");
			module_add_path(dir);
			free(dir);
		}
	}
	const char *env = getenv("MPLANGP_MODULES");
	while (env && *env) {
		const char *colon = strchr(env, ':');
		size_t n = colon ? (size_t)(colon - env) : strlen(env);
		char *dir = n > 0 ? malloc(n + 1) : NULL;
		if (dir) {
			memcpy(dir, env, n);
			dir[n] = '\0';
			module_add_path(dir);
			free(dir);
		}
		env = colon ? colon + 1 : NULL;
	}
	module_add_path("modules");
}

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "mplangp: cannot open '%s'\n", path);
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
		fprintf(stderr, "mplangp: cannot read '%s'\n", path);
		free(buf);
		return NULL;
	}
	buf[len] = '\0';
	return buf;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: mplangp <file> [args...]\n");
		return 1;
	}
	builtin_set_args(argc - 2, argv + 2);
	char *source = read_file(argv[1]);
	if (!source)
		return 1;

	Lexer *lx = lexer_new(source);
	if (!lx) {
		fprintf(stderr, "mplangp: out of memory\n");
		free(source);
		return 1;
	}
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
	add_module_paths(argv[1]);
	InterpretResult r = vm_run(&chunk);
	vm_free();
	chunk_free(&chunk);
	return r == INTERPRET_OK ? 0 : 70;
}
