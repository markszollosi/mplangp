#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "function.h"
#include "module.h"
#include "mplangpfunc.h"
#include "util.h"

Value value_none(void)
{
	Value v;
	v.type = VAL_NONE;
	return v;
}

Value value_bool(bool b)
{
	Value v;
	v.type = VAL_BOOL;
	v.as.b = b;
	return v;
}

Value value_int(int64_t i)
{
	Value v;
	v.type = VAL_INT;
	v.as.i = i;
	return v;
}

Value value_float(double f)
{
	Value v;
	v.type = VAL_FLOAT;
	v.as.f = f;
	return v;
}

Value value_string(const char *chars, int length)
{
	Value v;
	v.type = VAL_STRING;
	v.as.s = xmalloc((size_t)length + 1);
	memcpy(v.as.s, chars, (size_t)length);
	v.as.s[length] = '\0';
	return v;
}

Value value_copy(Value v)
{
	if (v.type == VAL_STRING)
		return value_string(v.as.s, (int)strlen(v.as.s));
	if (v.type == VAL_FUNC)
		v.as.fn->refs++;
	else if (v.type == VAL_LIST)
		v.as.list->refs++;
	else if (v.type == VAL_MAP)
		v.as.map->refs++;
	return v;
}

void value_free(Value v)
{
	if (v.type == VAL_STRING)
		free(v.as.s);
	else if (v.type == VAL_FUNC)
		function_release(v.as.fn);
	else if (v.type == VAL_LIST && --v.as.list->refs == 0) {
		for (int i = 0; i < v.as.list->count; i++)
			value_free(v.as.list->items[i]);
		free(v.as.list->items);
		free(v.as.list);
	} else if (v.type == VAL_MAP && --v.as.map->refs == 0) {
		for (int i = 0; i < v.as.map->count; i++) {
			free(v.as.map->keys[i]);
			value_free(v.as.map->vals[i]);
		}
		free(v.as.map->keys);
		free(v.as.map->vals);
		free(v.as.map);
	}
}

/* Stores val (owned) under key; the old value is released with value_free. */
void value_map_put(Value map, const char *key, Value val)
{
	int i = mp_find(map, key);
	if (i >= 0) {
		value_free(map.as.map->vals[i]);
		map.as.map->vals[i] = val;
	} else {
		mp_set(map, key, val);
	}
}

bool value_is_truthy(Value v)
{
	switch (v.type) {
	case VAL_NONE:
		return false;
	case VAL_BOOL:
		return v.as.b;
	case VAL_INT:
		return v.as.i != 0;
	case VAL_FLOAT:
		return v.as.f != 0.0;
	case VAL_STRING:
		return v.as.s[0] != '\0';
	case VAL_LIST:
		return v.as.list->count > 0;
	case VAL_MAP:
		return v.as.map->count > 0;
	case VAL_FUNC:
	case VAL_NATIVE:
	case VAL_MODULE:
		return true;
	}
	return false;
}

static bool equals(Value a, Value b, int depth)
{
	if (a.type != b.type)
		return false;
	if (depth > 64)
		return false;
	switch (a.type) {
	case VAL_NONE:
		return true;
	case VAL_BOOL:
		return a.as.b == b.as.b;
	case VAL_INT:
		return a.as.i == b.as.i;
	case VAL_FLOAT:
		return a.as.f == b.as.f;
	case VAL_STRING:
		return strcmp(a.as.s, b.as.s) == 0;
	case VAL_FUNC:
		return a.as.fn == b.as.fn;
	case VAL_NATIVE:
		return a.as.native == b.as.native;
	case VAL_MODULE:
		return a.as.module == b.as.module;
	case VAL_LIST:
		if (a.as.list == b.as.list)
			return true;
		if (a.as.list->count != b.as.list->count)
			return false;
		for (int i = 0; i < a.as.list->count; i++)
			if (!equals(a.as.list->items[i], b.as.list->items[i], depth + 1))
				return false;
		return true;
	case VAL_MAP:
		if (a.as.map == b.as.map)
			return true;
		if (a.as.map->count != b.as.map->count)
			return false;
		for (int i = 0; i < a.as.map->count; i++) {
			Value *o = mp_get(b, a.as.map->keys[i]);
			if (!o || !equals(a.as.map->vals[i], *o, depth + 1))
				return false;
		}
		return true;
	}
	return false;
}

bool value_equals(Value a, Value b)
{
	return equals(a, b, 0);
}

typedef struct {
	char *s;
	size_t n, cap;
} Buf;

static void buf_add(Buf *b, const char *chars, size_t n)
{
	if (b->n + n + 1 > b->cap) {
		b->cap = (b->n + n + 1) * 2;
		b->s = xrealloc(b->s, b->cap);
	}
	memcpy(b->s + b->n, chars, n);
	b->n += n;
	b->s[b->n] = '\0';
}

static void buf_str(Buf *b, const char *chars)
{
	buf_add(b, chars, strlen(chars));
}

static void quote(Buf *b, const char *str)
{
	buf_add(b, "\"", 1);
	for (const char *c = str; *c; c++) {
		if (*c == '"' || *c == '\\') {
			buf_add(b, "\\", 1);
			buf_add(b, c, 1);
		} else if (*c == '\n') {
			buf_str(b, "\\n");
		} else if (*c == '\r') {
			buf_str(b, "\\r");
		} else if (*c == '\t') {
			buf_str(b, "\\t");
		} else if ((unsigned char)*c < 0x20 || *c == 0x7f) {
			char esc[8];
			snprintf(esc, sizeof esc, "\\x%02x", (unsigned char)*c);
			buf_str(b, esc);
		} else {
			buf_add(b, c, 1);
		}
	}
	buf_add(b, "\"", 1);
}

static void format(Buf *b, Value v, bool quoted, int depth)
{
	char tmp[64];
	switch (v.type) {
	case VAL_NONE:
		buf_str(b, "None");
		break;
	case VAL_BOOL:
		buf_str(b, v.as.b ? "True" : "False");
		break;
	case VAL_INT:
		snprintf(tmp, sizeof tmp, "%lld", (long long)v.as.i);
		buf_str(b, tmp);
		break;
	case VAL_FLOAT:
		snprintf(tmp, sizeof tmp, "%.12g", v.as.f);
		if (strpbrk(tmp, ".eEnN") == NULL)
			strncat(tmp, ".0", sizeof tmp - strlen(tmp) - 1);
		buf_str(b, tmp);
		break;
	case VAL_STRING:
		if (quoted)
			quote(b, v.as.s);
		else
			buf_str(b, v.as.s);
		break;
	case VAL_FUNC:
		buf_str(b, "<function ");
		buf_str(b, v.as.fn->name);
		buf_str(b, ">");
		break;
	case VAL_NATIVE:
		buf_str(b, "<native ");
		buf_str(b, v.as.native->name);
		buf_str(b, ">");
		break;
	case VAL_MODULE:
		buf_str(b, "<module ");
		buf_str(b, v.as.module->name);
		buf_str(b, ">");
		break;
	case VAL_LIST:
		if (depth > 32) {
			buf_str(b, "[...]");
			break;
		}
		buf_add(b, "[", 1);
		for (int i = 0; i < v.as.list->count; i++) {
			if (i)
				buf_str(b, ", ");
			format(b, v.as.list->items[i], true, depth + 1);
		}
		buf_add(b, "]", 1);
		break;
	case VAL_MAP:
		if (depth > 32) {
			buf_str(b, "{...}");
			break;
		}
		buf_add(b, "{", 1);
		for (int i = 0; i < v.as.map->count; i++) {
			if (i)
				buf_str(b, ", ");
			quote(b, v.as.map->keys[i]);
			buf_str(b, ": ");
			format(b, v.as.map->vals[i], true, depth + 1);
		}
		buf_add(b, "}", 1);
		break;
	}
}

Value value_to_string(Value v)
{
	Buf b = { NULL, 0, 0 };
	format(&b, v, false, 0);
	Value r = value_string(b.s ? b.s : "", b.s ? (int)b.n : 0);
	free(b.s);
	return r;
}

void value_print(Value v)
{
	Buf b = { NULL, 0, 0 };
	format(&b, v, false, 0);
	if (b.s)
		fputs(b.s, stdout);
	free(b.s);
}

void value_array_init(ValueArray *a)
{
	a->values = NULL;
	a->count = 0;
	a->capacity = 0;
}

void value_array_write(ValueArray *a, Value v)
{
	if (a->capacity < a->count + 1) {
		int old_capacity = a->capacity;
		a->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
		a->values = xrealloc(a->values, sizeof(Value) * (size_t)a->capacity);
	}
	a->values[a->count] = v;
	a->count++;
}

void value_array_free(ValueArray *a)
{
	for (int i = 0; i < a->count; i++)
		value_free(a->values[i]);
	free(a->values);
	value_array_init(a);
}
