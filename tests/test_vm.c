#include <stdio.h>
#include <string.h>
#include "vm.h"
#include "opcode.h"

static int failures;

static void emit(Chunk *c, int b)
{
	chunk_write(c, (uint8_t)b, 1);
}

static void emit_k(Chunk *c, int op, Value v)
{
	emit(c, op);
	emit(c, chunk_add_constant(c, v));
}

static void emit_jump(Chunk *c, int op, int off)
{
	emit(c, op);
	emit(c, off >> 8);
	emit(c, off & 0xff);
}

static InterpretResult run(Chunk *c, char *out, int size)
{
	fflush(stdout);
	FILE *f = freopen("/tmp/test_vm_out.txt", "w", stdout);
	InterpretResult r = vm_run(c);
	fflush(stdout);
	fclose(f);
	f = fopen("/tmp/test_vm_out.txt", "r");
	size_t n = fread(out, 1, size - 1, f);
	out[n] = '\0';
	fclose(f);
	return r;
}

static void check(const char *name, InterpretResult got, InterpretResult want, const char *out, const char *expect)
{
	if (got != want || strcmp(out, expect) != 0) {
		fprintf(stderr, "FAIL %s: result %d out '%s' (want %d '%s')\n", name, got, out, want, expect);
		failures++;
	}
}

static void binary(const char *name, Value a, Value b, int op, InterpretResult want, const char *expect)
{
	char out[256];
	Chunk c;
	chunk_init(&c);
	emit_k(&c, OP_CONST, a);
	emit_k(&c, OP_CONST, b);
	emit(&c, op);
	emit(&c, OP_PRINT);
	emit(&c, OP_HALT);
	vm_init();
	InterpretResult r = run(&c, out, sizeof out);
	check(name, r, want, out, expect);
	vm_free();
	chunk_free(&c);
}

static void test_globals_and_loop(void)
{
	char out[256];
	Chunk c;
	chunk_init(&c);
	int i = chunk_add_constant(&c, value_string("i", 1));
	int zero = chunk_add_constant(&c, value_int(0));
	int three = chunk_add_constant(&c, value_int(3));
	int one = chunk_add_constant(&c, value_int(1));
	emit(&c, OP_CONST); emit(&c, zero);
	emit(&c, OP_SET_GLOBAL); emit(&c, i);
	int top = c.count;
	emit(&c, OP_GET_GLOBAL); emit(&c, i);
	emit(&c, OP_CONST); emit(&c, three);
	emit(&c, OP_LT);
	emit_jump(&c, OP_JUMP_IF_FALSE, 14);
	emit(&c, OP_POP);
	emit(&c, OP_GET_GLOBAL); emit(&c, i);
	emit(&c, OP_PRINT);
	emit(&c, OP_GET_GLOBAL); emit(&c, i);
	emit(&c, OP_CONST); emit(&c, one);
	emit(&c, OP_ADD);
	emit(&c, OP_SET_GLOBAL); emit(&c, i);
	emit_jump(&c, OP_LOOP, c.count + 3 - top);
	emit(&c, OP_POP);
	emit(&c, OP_HALT);
	vm_init();
	InterpretResult r = run(&c, out, sizeof out);
	check("while", r, INTERPRET_OK, out, "0\n1\n2\n");
	vm_free();
	chunk_free(&c);
}

static void test_undefined(void)
{
	char out[64];
	Chunk c;
	chunk_init(&c);
	emit_k(&c, OP_GET_GLOBAL, value_string("zz", 2));
	emit(&c, OP_HALT);
	vm_init();
	InterpretResult r = run(&c, out, sizeof out);
	check("undefined", r, INTERPRET_RUNTIME_ERROR, out, "");
	vm_free();
	chunk_free(&c);
}

static void test_underflow(void)
{
	char out[64];
	Chunk c;
	chunk_init(&c);
	emit(&c, OP_POP);
	emit(&c, OP_HALT);
	vm_init();
	InterpretResult r = run(&c, out, sizeof out);
	check("underflow", r, INTERPRET_RUNTIME_ERROR, out, "");
	vm_free();
	chunk_free(&c);
}

int main(void)
{
	binary("add int", value_int(2), value_int(3), OP_ADD, INTERPRET_OK, "5\n");
	binary("div float", value_int(7), value_int(2), OP_DIV, INTERPRET_OK, "3.5\n");
	binary("mixed", value_int(1), value_float(2.5), OP_ADD, INTERPRET_OK, "3.5\n");
	binary("mod neg", value_int(7), value_int(-3), OP_MOD, INTERPRET_OK, "-2\n");
	binary("mod neg2", value_int(-7), value_int(3), OP_MOD, INTERPRET_OK, "2\n");
	binary("bool add", value_bool(true), value_int(2), OP_ADD, INTERPRET_OK, "3\n");
	binary("concat", value_string("foo", 3), value_string("bar", 3), OP_ADD, INTERPRET_OK, "foobar\n");
	binary("repeat", value_string("ab", 2), value_int(3), OP_MUL, INTERPRET_OK, "ababab\n");
	binary("repeat rev", value_int(2), value_string("x", 1), OP_MUL, INTERPRET_OK, "xx\n");
	binary("repeat neg", value_string("ab", 2), value_int(-1), OP_MUL, INTERPRET_OK, "\n");
	binary("repeat huge", value_string("ab", 2), value_int(INT64_MAX), OP_MUL, INTERPRET_RUNTIME_ERROR, "");
	binary("add ovf", value_int(INT64_MAX), value_int(1), OP_ADD, INTERPRET_RUNTIME_ERROR, "");
	binary("sub ovf", value_int(INT64_MIN), value_int(1), OP_SUB, INTERPRET_RUNTIME_ERROR, "");
	binary("mul ovf", value_int(INT64_MAX), value_int(2), OP_MUL, INTERPRET_RUNTIME_ERROR, "");
	binary("min mul -1", value_int(INT64_MIN), value_int(-1), OP_MUL, INTERPRET_RUNTIME_ERROR, "");
	binary("min mod -1", value_int(INT64_MIN), value_int(-1), OP_MOD, INTERPRET_OK, "0\n");
	binary("max add 0", value_int(INT64_MAX), value_int(0), OP_ADD, INTERPRET_OK, "9223372036854775807\n");
	binary("div0", value_int(1), value_int(0), OP_DIV, INTERPRET_RUNTIME_ERROR, "");
	binary("mod0", value_int(1), value_int(0), OP_MOD, INTERPRET_RUNTIME_ERROR, "");
	binary("fdiv0", value_float(1.0), value_float(0.0), OP_DIV, INTERPRET_RUNTIME_ERROR, "");
	binary("none add", value_none(), value_int(1), OP_ADD, INTERPRET_RUNTIME_ERROR, "");
	binary("str+int", value_string("a", 1), value_int(1), OP_ADD, INTERPRET_RUNTIME_ERROR, "");
	binary("eq mixed", value_int(1), value_float(1.0), OP_EQ, INTERPRET_OK, "True\n");
	binary("ne", value_int(1), value_int(2), OP_NE, INTERPRET_OK, "True\n");
	binary("eq types", value_int(1), value_string("1", 1), OP_EQ, INTERPRET_OK, "False\n");
	binary("lt", value_int(1), value_float(1.5), OP_LT, INTERPRET_OK, "True\n");
	binary("ge str", value_string("a", 1), value_string("b", 1), OP_GE, INTERPRET_OK, "False\n");
	binary("lt bad", value_string("a", 1), value_int(1), OP_LT, INTERPRET_RUNTIME_ERROR, "");
	test_globals_and_loop();
	test_undefined();
	test_underflow();
	fprintf(stderr, failures ? "%d failures\n" : "all passed\n", failures);
	return failures != 0;
}
