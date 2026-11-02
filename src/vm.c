#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm.h"
#include "function.h"
#include "module.h"
#include "opcode.h"
#include "table.h"
#include "builtin.h"
#include "util.h"

#define STACK_MAX 65536
#define FRAMES_MAX 10000

typedef struct {
	Function *fn;
	uint8_t *ip;
	int base; /* stack index of the callee; locals follow it */
} Frame;

static Value stack[STACK_MAX];
static int sp;
static Frame frames[FRAMES_MAX];
static int nframes;
static Table *globals;

void vm_init(void)
{
	sp = 0;
	globals = table_new();
	builtin_install(globals);
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
	module_free_all();
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
	if (op == OP_ADD && a.type == VAL_LIST && b.type == VAL_LIST) {
		Value r = mp_list();
		for (int i = 0; i < a.as.list->count; i++)
			mp_push(r, value_copy(a.as.list->items[i]));
		for (int i = 0; i < b.as.list->count; i++)
			mp_push(r, value_copy(b.as.list->items[i]));
		*out = r;
		return NULL;
	}
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

static char *errbuf;

static const char *errf(const char *fmt, const char *a, const char *b)
{
	int n = snprintf(NULL, 0, fmt, a, b);
	char *p = n < 0 ? NULL : realloc(errbuf, (size_t)n + 1);
	if (!p)
		return "Out of memory.";
	errbuf = p;
	snprintf(errbuf, (size_t)n + 1, fmt, a, b);
	return errbuf;
}

static const char *index_get(Value obj, Value idx, Value *out)
{
	if (obj.type == VAL_MAP) {
		if (idx.type != VAL_STRING)
			return "Map keys must be strings.";
		Value *v = mp_get(obj, idx.as.s);
		if (!v) {
			return errf("Key '%s' not found.", idx.as.s, "");
		}
		*out = value_copy(*v);
		return NULL;
	}
	if (obj.type != VAL_LIST && obj.type != VAL_STRING)
		return "Only lists, maps and strings can be indexed.";
	if (!is_intlike(idx))
		return "Index must be an integer.";
	int64_t n = obj.type == VAL_LIST ? obj.as.list->count : (int64_t)strlen(obj.as.s);
	int64_t i = as_int(idx);
	if (i < 0)
		i += n;
	if (i < 0 || i >= n)
		return "Index out of range.";
	*out = obj.type == VAL_LIST ? value_copy(obj.as.list->items[i]) : value_string(obj.as.s + i, 1);
	return NULL;
}

static const char *index_set(Value obj, Value idx, Value val)
{
	if (obj.type == VAL_MAP) {
		if (idx.type != VAL_STRING)
			return "Map keys must be strings.";
		value_map_put(obj, idx.as.s, value_copy(val));
		return NULL;
	}
	if (obj.type != VAL_LIST)
		return "Only lists and maps support assignment.";
	if (!is_intlike(idx))
		return "Index must be an integer.";
	int64_t i = as_int(idx);
	if (i < 0)
		i += obj.as.list->count;
	if (i < 0 || i >= obj.as.list->count)
		return "Index out of range.";
	value_free(obj.as.list->items[i]);
	obj.as.list->items[i] = value_copy(val);
	return NULL;
}

static const char *contains(Value needle, Value hay, bool *out)
{
	*out = false;
	if (hay.type == VAL_LIST) {
		for (int i = 0; i < hay.as.list->count && !*out; i++)
			*out = values_equal(needle, hay.as.list->items[i]);
		return NULL;
	}
	if (needle.type != VAL_STRING)
		return hay.type == VAL_MAP ? "Map keys must be strings." : "'in <string>' needs a string.";
	if (hay.type == VAL_MAP)
		*out = mp_find(hay, needle.as.s) >= 0;
	else if (hay.type == VAL_STRING)
		*out = strstr(hay.as.s, needle.as.s) != NULL;
	else
		return "Argument of 'in' is not iterable.";
	return NULL;
}

InterpretResult vm_run(Chunk *main_chunk)
{
	Function script;
	script.refs = 1;
	script.name = NULL;
	script.arity = 0;
	script.nlocals = 0;
	script.chunk = *main_chunk;
	stack_reset();
	nframes = 1;
	Frame *f = &frames[0];
	f->fn = &script;
	f->ip = script.chunk.code;
	f->base = 0;
	Chunk *chunk;
	uint8_t *ip, *end;
#define LOAD() (chunk = &f->fn->chunk, ip = f->ip, end = chunk->code + chunk->count)
	LOAD();

	for (;;) {
		uint8_t *at = ip;
		if (ip >= end) {
			return error(chunk, end - 1, "%s", "Unexpected end of code.");
		}
		OpCode op = (OpCode)*ip++;
		bool wide = false;
		if (op == OP_WIDE) {
			if (ip >= end) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			wide = true;
			op = (OpCode)*ip++;
		}
		switch (op) {
		case OP_CONST:
		case OP_GET_GLOBAL:
		case OP_SET_GLOBAL:
		case OP_DEFINE_GLOBAL: {
			if (end - ip < (wide ? 2 : 1)) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int idx = wide ? (ip[0] << 8) | ip[1] : ip[0];
			ip += wide ? 2 : 1;
			if (idx >= chunk->constants.count) {
				return error(chunk, at, "%s", "Bad constant index.");
			}
			Value c = chunk->constants.values[idx];
			if (op == OP_CONST) {
				if (sp >= STACK_MAX) {
					return error(chunk, at, "%s", "Stack overflow.");
				}
				stack[sp++] = value_copy(c);
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
				stack[sp++] = value_copy(v);
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
		case OP_IN: {
			if (sp < 2) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			bool r;
			const char *msg = contains(stack[sp - 2], stack[sp - 1], &r);
			if (msg) {
				return error(chunk, at, "%s", msg);
			}
			value_free(stack[--sp]);
			value_free(stack[sp - 1]);
			stack[sp - 1] = value_bool(r);
			break;
		}
		case OP_LIST:
		case OP_MAP: {
			if (ip >= end) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int n = *ip++, take = op == OP_MAP ? n * 2 : n;
			if (sp < take) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value *base = &stack[sp - take];
			if (op == OP_MAP) {
				for (int i = 0; i < n; i++) {
					if (base[i * 2].type != VAL_STRING) {
						return error(chunk, at, "%s", "Map keys must be strings.");
					}
				}
			}
			Value r = op == OP_MAP ? mp_map() : mp_list();
			for (int i = 0; i < n; i++) {
				if (op == OP_LIST) {
					mp_push(r, base[i]);
					continue;
				}
				value_map_put(r, base[i * 2].as.s, base[i * 2 + 1]);
				value_free(base[i * 2]);
			}
			sp -= take;
			stack[sp++] = r;
			break;
		}
		case OP_INDEX: {
			if (sp < 2) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value r;
			const char *msg = index_get(stack[sp - 2], stack[sp - 1], &r);
			if (msg) {
				return error(chunk, at, "%s", msg);
			}
			value_free(stack[--sp]);
			value_free(stack[sp - 1]);
			stack[sp - 1] = r;
			break;
		}
		case OP_SET_INDEX: {
			if (sp < 3) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			const char *msg = index_set(stack[sp - 3], stack[sp - 2], stack[sp - 1]);
			if (msg) {
				return error(chunk, at, "%s", msg);
			}
			value_free(stack[--sp]);
			value_free(stack[--sp]);
			value_free(stack[--sp]);
			break;
		}
		case OP_ITER_HAS:
		case OP_ITER_GET: {
			if (sp < 2) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value it = stack[sp - 2];
			int64_t i = as_int(stack[sp - 1]);
			int64_t n;
			if (it.type == VAL_LIST) {
				n = it.as.list->count;
			} else if (it.type == VAL_MAP) {
				n = it.as.map->count;
			} else if (it.type == VAL_STRING) {
				n = (int64_t)strlen(it.as.s);
			} else {
				return error(chunk, at, "%s", "Can only loop over lists, maps and strings.");
			}
			Value r;
			if (op == OP_ITER_HAS) {
				r = value_bool(i < n);
			} else if (i >= n) {
				return error(chunk, at, "%s", "Iterator out of range.");
			} else if (it.type == VAL_LIST) {
				r = value_copy(it.as.list->items[i]);
			} else if (it.type == VAL_MAP) {
				r = value_string(it.as.map->keys[i], (int)strlen(it.as.map->keys[i]));
			} else {
				r = value_string(it.as.s + i, 1);
			}
			value_free(stack[--sp]);
			value_free(stack[sp - 1]);
			stack[sp - 1] = r;
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
		case OP_GET_LOCAL:
		case OP_SET_LOCAL: {
			if (ip >= end) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int slot = *ip++;
			if (slot < 1 || slot > f->fn->nlocals) {
				return error(chunk, at, "%s", "Bad local slot.");
			}
			if (op == OP_GET_LOCAL) {
				if (sp >= STACK_MAX) {
					return error(chunk, at, "%s", "Stack overflow.");
				}
				stack[sp] = value_copy(stack[f->base + slot]);
				sp++;
			} else {
				if (sp < 1) {
					return error(chunk, at, "%s", "Stack underflow.");
				}
				value_free(stack[f->base + slot]);
				stack[f->base + slot] = stack[--sp];
			}
			break;
		}
		case OP_CALL: {
			if (ip >= end) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int argc = *ip++;
			if (sp < argc + 1) {
				return error(chunk, at, "%s", "Stack underflow.");
			}
			Value callee = stack[sp - 1 - argc];
			if (callee.type == VAL_NATIVE) {
				Value ret = value_none();
				if (!callee.as.native->fn(argc, &stack[sp - argc], &ret)) {
					InterpretResult res = error(chunk, at, "%s",
					    ret.type == VAL_STRING ? ret.as.s : "Native function failed.");
					value_free(ret);
					return res;
				}
				while (argc-- > 0) {
					value_free(stack[--sp]);
				}
				stack[sp - 1] = ret;
				break;
			}
			if (callee.type != VAL_FUNC) {
				return error(chunk, at, "%s", "Can only call functions.");
			}
			Function *fn = callee.as.fn;
			if (argc != fn->arity) {
				char msg[64];
				snprintf(msg, sizeof msg, "Expected %d arguments but got %d.", fn->arity, argc);
				return error(chunk, at, "%s", msg);
			}
			if (nframes >= FRAMES_MAX) {
				return error(chunk, at, "%s", "Stack overflow: calls nested too deeply (limit 9999).");
			}
			if (sp + fn->nlocals - argc > STACK_MAX) {
				return error(chunk, at, "%s", "Stack overflow: value stack full (65536 slots).");
			}
			f->ip = ip;
			Frame *nf = &frames[nframes++];
			nf->fn = fn;
			nf->ip = fn->chunk.code;
			nf->base = sp - 1 - argc;
			for (int i = argc; i < fn->nlocals; i++) {
				stack[sp++] = value_none();
			}
			f = nf;
			LOAD();
			break;
		}
		case OP_RETURN: {
			if (nframes < 2 || sp < 1) {
				return error(chunk, at, "%s", "Return outside function.");
			}
			Value result = stack[--sp];
			while (sp > f->base) {
				value_free(stack[--sp]);
			}
			stack[sp++] = result;
			nframes--;
			f = &frames[nframes - 1];
			LOAD();
			break;
		}
		case OP_IMPORT:
		case OP_GET_ATTR: {
			if (end - ip < (wide ? 2 : 1)) {
				return error(chunk, at, "%s", "Truncated instruction.");
			}
			int idx = wide ? (ip[0] << 8) | ip[1] : ip[0];
			ip += wide ? 2 : 1;
			if (idx >= chunk->constants.count || chunk->constants.values[idx].type != VAL_STRING) {
				return error(chunk, at, "%s", "Bad constant index.");
			}
			const char *name = chunk->constants.values[idx].as.s;
			char msg[256];
			if (op == OP_IMPORT) {
				Module *m = module_load(name, msg, sizeof msg);
				if (!m) {
					return error(chunk, at, "%s", msg);
				}
				if (sp >= STACK_MAX) {
					return error(chunk, at, "%s", "Stack overflow.");
				}
				stack[sp].type = VAL_MODULE;
				stack[sp++].as.module = m;
			} else {
				if (sp < 1) {
					return error(chunk, at, "%s", "Stack underflow.");
				}
				if (stack[sp - 1].type != VAL_MODULE) {
					return error(chunk, at, "%s", "Only modules have attributes.");
				}
				Value member;
				if (!module_lookup(stack[sp - 1].as.module, name, &member)) {
					return error(chunk, at, "%s", errf("Module '%s' has no attribute '%s'.",
					    stack[sp - 1].as.module->name, name));
				}
				stack[sp - 1] = member;
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
