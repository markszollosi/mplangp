#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "opcode.h"
#include "table.h"
#include "util.h"

#define STACK_MAX 256

static Value stack[STACK_MAX];
static int sp;
static Table *globals;

void vm_init(void)
{
	sp = 0;
	globals = table_new();
}

static void stack_reset(void)
{
	while (sp > 0) {
		value_free(stack[--sp]);
	}
}

void vm_free(void)
{
	stack_reset();
	if (globals) {
		table_free(globals);
		globals = NULL;
	}
}

static bool is_intlike(Value v)
{
	return v.type == VAL_INT || v.type == VAL_BOOL;
}

static bool is_num(Value v)
{
	return is_intlike(v) || v.type == VAL_FLOAT;
}

static int64_t as_int(Value v)
{
	return v.type == VAL_BOOL ? v.as.b : v.as.i;
}

static double as_float(Value v)
{
	return v.type == VAL_FLOAT ? v.as.f : (double)as_int(v);
}

static Value copy(Value v)
{
	return v.type == VAL_STRING ? value_string(v.as.s, (int)strlen(v.as.s)) : v;
}

static bool repeat(Value s, int64_t n, Value *out)
{
	size_t len = strlen(s.as.s);
	if (n < 0) {
		n = 0;
	}
	if (len > 0 && (uint64_t)n > (uint64_t)(INT_MAX - 1) / len) {
		return false;
	}
	if (len == 0) {
		n = 0;
	}
	char *buf = xmalloc(len * (size_t)n + 1);
	for (int64_t i = 0; i < n; i++) {
		memcpy(buf + len * (size_t)i, s.as.s, len);
	}
	buf[len * (size_t)n] = '\0';
	*out = value_string(buf, (int)(len * (size_t)n));
	free(buf);
	return true;
}

static bool concat(Value a, Value b, Value *out)
{
	size_t la = strlen(a.as.s), lb = strlen(b.as.s);
	if (la + lb >= INT_MAX) {
		return false;
	}
	char *buf = xmalloc(la + lb + 1);
	memcpy(buf, a.as.s, la);
	memcpy(buf + la, b.as.s, lb + 1);
	*out = value_string(buf, (int)(la + lb));
	free(buf);
	return true;
}

static int compare(Value a, Value b)
{
	if (a.type == VAL_STRING) {
		int c = strcmp(a.as.s, b.as.s);
		return (c > 0) - (c < 0);
	}
	if (is_intlike(a) && is_intlike(b)) {
		int64_t x = as_int(a), y = as_int(b);
		return (x > y) - (x < y);
	}
	double x = as_float(a), y = as_float(b);
	return (x > y) - (x < y);
}

static bool values_equal(Value a, Value b)
{
	if (is_num(a) && is_num(b)) {
		return compare(a, b) == 0;
	}
	return value_equals(a, b);
}

static InterpretResult error(Chunk *chunk, uint8_t *at, const char *fmt, const char *arg)
{
	fprintf(stderr, "line %d: ", chunk->count > 0 ? chunk->lines[at - chunk->code] : 0);
	fprintf(stderr, fmt, arg);
	fputc('\n', stderr);
	stack_reset();
	return INTERPRET_RUNTIME_ERROR;
}

static const char *arith(OpCode op, Value a, Value b, Value *out)
{
	if (op == OP_ADD && a.type == VAL_STRING && b.type == VAL_STRING) {
		return concat(a, b, out) ? NULL : "String too long.";
	}
	if (op == OP_MUL && a.type == VAL_STRING && is_intlike(b)) {
		return repeat(a, as_int(b), out) ? NULL : "String too long.";
	}
	if (op == OP_MUL && is_intlike(a) && b.type == VAL_STRING) {
		return repeat(b, as_int(a), out) ? NULL : "String too long.";
	}
	if (!is_num(a) || !is_num(b)) {
		return "Unsupported operand types.";
	}
	if (is_intlike(a) && is_intlike(b) && op != OP_DIV) {
		int64_t x = as_int(a), y = as_int(b), r;
		switch (op) {
		case OP_ADD: r = (int64_t)((uint64_t)x + (uint64_t)y); break;
		case OP_SUB: r = (int64_t)((uint64_t)x - (uint64_t)y); break;
		case OP_MUL: r = (int64_t)((uint64_t)x * (uint64_t)y); break;
		default:
			if (y == 0) {
				return "Division by zero.";
			}
			if (y == -1) {
				r = 0;
			} else {
				r = x % y;
				if (r != 0 && ((r < 0) != (y < 0))) {
					r += y;
				}
			}
			break;
		}
		*out = value_int(r);
		return NULL;
	}
	double x = as_float(a), y = as_float(b), r;
	switch (op) {
	case OP_ADD: r = x + y; break;
	case OP_SUB: r = x - y; break;
	case OP_MUL: r = x * y; break;
	case OP_DIV:
		if (y == 0) {
			return "Division by zero.";
		}
		r = x / y;
		break;
	default:
		if (y == 0) {
			return "Division by zero.";
		}
		r = fmod(x, y);
		if (r != 0 && ((r < 0) != (y < 0))) {
			r += y;
		}
		break;
	}
	*out = value_float(r);
	return NULL;
}

InterpretResult vm_run(Chunk *chunk)
{
	uint8_t *ip = chunk->code;
	uint8_t *end = chunk->code + chunk->count;
	stack_reset();

	for (;;) {
		uint8_t *at = ip;
		if (ip >= end) {
			return error(chunk, end - 1, "%s", "Unexpected end of code.");
		}
		OpCode op = (OpCode)*ip++;
		switch (op) {
		case OP_CONST:
		case OP_GET_GLOBAL:
		case OP_SET_GLOBAL:
		case OP_DEFINE_GLOBAL: {
			if (ip >= end) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int idx = *ip++;
			if (idx >= chunk->constants.count) {
				return error(chunk, at, "%s", "Bad constant index.");
			}
			Value c = chunk->constants.values[idx];
			if (op == OP_CONST) {
				if (sp >= STACK_MAX) {
					return error(chunk, at, "%s", "Stack overflow.");
				}
				stack[sp++] = copy(c);
				break;
			}
			if (c.type != VAL_STRING) {
				return error(chunk, at, "%s", "Bad variable name.");
			}
			int len = (int)strlen(c.as.s);
			if (op == OP_GET_GLOBAL) {
				Value v;
				if (!table_get(globals, c.as.s, len, &v)) {
					return error(chunk, at, "Undefined variable '%s'.", c.as.s);
				}
				if (sp >= STACK_MAX) {
					return error(chunk, at, "%s", "Stack overflow.");
				}
				stack[sp++] = copy(v);
			} else {
				if (sp < 1) {
					return error(chunk, at, "%s", "Stack underflow.");
				}
				table_set(globals, c.as.s, len, stack[--sp]);
			}
			break;
		}
		case OP_POP:
			if (sp < 1) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			value_free(stack[--sp]);
			break;
		case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD: {
			if (sp < 2) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value r;
			const char *msg = arith(op, stack[sp - 2], stack[sp - 1], &r);
			if (msg) {
				return error(chunk, at, "%s", msg);
			}
			value_free(stack[--sp]);
			value_free(stack[sp - 1]);
			stack[sp - 1] = r;
			break;
		}
		case OP_NEG:
			if (sp < 1) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			if (!is_num(stack[sp - 1])) {
				return error(chunk, at, "%s", "Unsupported operand types.");
			}
			stack[sp - 1] = stack[sp - 1].type == VAL_FLOAT ? value_float(-stack[sp - 1].as.f)
				: value_int((int64_t)(0 - (uint64_t)as_int(stack[sp - 1])));
			break;
		case OP_NOT: {
			if (sp < 1) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			bool t = value_is_truthy(stack[sp - 1]);
			value_free(stack[sp - 1]);
			stack[sp - 1] = value_bool(!t);
			break;
		}
		case OP_EQ: case OP_NE: case OP_LT: case OP_LE: case OP_GT: case OP_GE: {
			if (sp < 2) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value a = stack[sp - 2], b = stack[sp - 1];
			bool r;
			if (op == OP_EQ || op == OP_NE) {
				r = values_equal(a, b) == (op == OP_EQ);
			} else {
				if (!((is_num(a) && is_num(b)) || (a.type == VAL_STRING && b.type == VAL_STRING))) {
					return error(chunk, at, "%s", "Unsupported operand types.");
				}
				int c = compare(a, b);
				r = op == OP_LT ? c < 0 : op == OP_LE ? c <= 0 : op == OP_GT ? c > 0 : c >= 0;
			}
			value_free(a);
			value_free(b);
			sp--;
			stack[sp - 1] = value_bool(r);
			break;
		}
		case OP_PRINT:
			if (sp < 1) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			value_print(stack[sp - 1]);
			putchar('\n');
			value_free(stack[--sp]);
			break;
		case OP_JUMP:
		case OP_JUMP_IF_FALSE:
		case OP_LOOP: {
			if (end - ip < 2) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int off = (ip[0] << 8) | ip[1];
			ip += 2;
			if (op == OP_LOOP) {
				if (off > ip - chunk->code) {
					return error(chunk, at, "%s", "Bad jump.");
				}
				ip -= off;
			} else if (op == OP_JUMP) {
				ip += off;
			} else {
				if (sp < 1) {
					return error(chunk, at, "%s", "Stack underflow.");
				}
				if (!value_is_truthy(stack[sp - 1])) {
					ip += off;
				}
			}
			break;
		}
		case OP_HALT:
			return INTERPRET_OK;
		default:
			return error(chunk, at, "%s", "Unknown opcode.");
		}
	}
}
