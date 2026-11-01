#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "builtin.h"
#include "mplangpfunc.h"

#define RANGE_MAX 10000000

static bool b_len(int argc, Value *argv, Value *ret)
{
	if (argc != 1)
		return mp_error(ret, "len(x) takes one argument.");
	switch (argv[0].type) {
	case VAL_STRING: *ret = mp_int((int64_t)strlen(argv[0].as.s)); return true;
	case VAL_LIST: *ret = mp_int(argv[0].as.list->count); return true;
	case VAL_MAP: *ret = mp_int(argv[0].as.map->count); return true;
	default: return mp_error(ret, "len(x) takes a string, list or map.");
	}
}

static bool b_push(int argc, Value *argv, Value *ret)
{
	if (argc != 2 || argv[0].type != VAL_LIST)
		return mp_error(ret, "push(list, x) takes a list and a value.");
	mp_push(argv[0], value_copy(argv[1]));
	return true;
}

static bool b_pop(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_LIST)
		return mp_error(ret, "pop(list) takes a list.");
	if (argv[0].as.list->count == 0)
		return mp_error(ret, "pop from empty list.");
	*ret = argv[0].as.list->items[--argv[0].as.list->count];
	return true;
}

static bool b_keys(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_MAP)
		return mp_error(ret, "keys(map) takes a map.");
	*ret = mp_list();
	for (int i = 0; i < argv[0].as.map->count; i++)
		mp_push(*ret, mp_string(argv[0].as.map->keys[i], (int)strlen(argv[0].as.map->keys[i])));
	return true;
}

static bool b_values(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_MAP)
		return mp_error(ret, "values(map) takes a map.");
	*ret = mp_list();
	for (int i = 0; i < argv[0].as.map->count; i++)
		mp_push(*ret, value_copy(argv[0].as.map->vals[i]));
	return true;
}

static bool b_has(int argc, Value *argv, Value *ret)
{
	if (argc != 2 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING)
		return mp_error(ret, "has(map, key) takes a map and a string.");
	*ret = mp_bool(mp_find(argv[0], argv[1].as.s) >= 0);
	return true;
}

static bool b_get(int argc, Value *argv, Value *ret)
{
	if (argc != 3 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING)
		return mp_error(ret, "get(map, key, default) takes a map, a string and a value.");
	Value *v = mp_get(argv[0], argv[1].as.s);
	*ret = value_copy(v ? *v : argv[2]);
	return true;
}

static bool b_remove(int argc, Value *argv, Value *ret)
{
	if (argc != 2 || argv[0].type != VAL_MAP || argv[1].type != VAL_STRING)
		return mp_error(ret, "remove(map, key) takes a map and a string.");
	Map *m = argv[0].as.map;
	int i = mp_find(argv[0], argv[1].as.s);
	if (i < 0)
		return true;
	free(m->keys[i]);
	value_free(m->vals[i]);
	for (; i + 1 < m->count; i++) {
		m->keys[i] = m->keys[i + 1];
		m->vals[i] = m->vals[i + 1];
	}
	m->count--;
	return true;
}

static bool b_type(int argc, Value *argv, Value *ret)
{
	static const char *const names[] = {
		"none", "bool", "int", "float", "str", "function", "function", "module", "list", "map"
	};
	if (argc != 1)
		return mp_error(ret, "type(x) takes one argument.");
	const char *n = names[argv[0].type];
	*ret = mp_string(n, (int)strlen(n));
	return true;
}

static bool b_text(int argc, Value *argv, Value *ret)
{
	if (argc != 1)
		return mp_error(ret, "text(x) takes one argument.");
	*ret = value_to_string(argv[0]);
	return true;
}

static bool b_int(int argc, Value *argv, Value *ret)
{
	if (argc != 1)
		return mp_error(ret, "int(x) takes one argument.");
	if (mp_is_int(argv[0])) {
		*ret = mp_int(mp_to_int(argv[0]));
	} else if (argv[0].type == VAL_FLOAT) {
		double f = argv[0].as.f;
		if (f != f || f >= 9.2e18 || f <= -9.2e18)
			return mp_error(ret, "int(x) out of range.");
		*ret = mp_int((int64_t)f);
	} else if (argv[0].type == VAL_STRING) {
		char *end;
		errno = 0;
		long long v = strtoll(argv[0].as.s, &end, 10);
		if (end == argv[0].as.s || *end || errno)
			return mp_error(ret, "int(x) cannot parse the string.");
		*ret = mp_int(v);
	} else {
		return mp_error(ret, "int(x) takes a number or string.");
	}
	return true;
}

static bool b_float(int argc, Value *argv, Value *ret)
{
	if (argc != 1)
		return mp_error(ret, "float(x) takes one argument.");
	if (mp_is_num(argv[0])) {
		*ret = mp_float(mp_to_float(argv[0]));
	} else if (argv[0].type == VAL_STRING) {
		char *end;
		double v = strtod(argv[0].as.s, &end);
		if (end == argv[0].as.s || *end)
			return mp_error(ret, "float(x) cannot parse the string.");
		*ret = mp_float(v);
	} else {
		return mp_error(ret, "float(x) takes a number or string.");
	}
	return true;
}

static bool b_range(int argc, Value *argv, Value *ret)
{
	int64_t lo = 0, hi;
	if (argc == 1 && mp_is_int(argv[0])) {
		hi = mp_to_int(argv[0]);
	} else if (argc == 2 && mp_is_int(argv[0]) && mp_is_int(argv[1])) {
		lo = mp_to_int(argv[0]);
		hi = mp_to_int(argv[1]);
	} else {
		return mp_error(ret, "range(n) or range(a, b) takes integers.");
	}
	if (hi - lo > RANGE_MAX)
		return mp_error(ret, "range is too large.");
	*ret = mp_list();
	for (int64_t i = lo; i < hi; i++)
		mp_push(*ret, mp_int(i));
	return true;
}

static int cmp(const void *a, const void *b)
{
	const Value *x = a, *y = b;
	if (x->type == VAL_STRING)
		return strcmp(x->as.s, y->as.s);
	double p = mp_to_float(*x), q = mp_to_float(*y);
	return (p > q) - (p < q);
}

static bool b_sort(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_LIST)
		return mp_error(ret, "sort(list) takes a list.");
	List *l = argv[0].as.list;
	for (int i = 0; i < l->count; i++) {
		if (l->items[i].type != VAL_STRING && !mp_is_num(l->items[i]))
			return mp_error(ret, "sort(list) needs numbers or strings.");
		if ((l->items[i].type == VAL_STRING) != (l->items[0].type == VAL_STRING))
			return mp_error(ret, "sort(list) cannot mix numbers and strings.");
	}
	if (l->count > 1)
		qsort(l->items, (size_t)l->count, sizeof *l->items, cmp);
	return true;
}

static bool b_slice(int argc, Value *argv, Value *ret)
{
	if (argc != 3 || !mp_is_int(argv[1]) || !mp_is_int(argv[2]) ||
	    (argv[0].type != VAL_LIST && argv[0].type != VAL_STRING))
		return mp_error(ret, "slice(x, from, to) takes a list or string and two integers.");
	int64_t n = argv[0].type == VAL_LIST ? argv[0].as.list->count : (int64_t)strlen(argv[0].as.s);
	int64_t a = mp_to_int(argv[1]), b = mp_to_int(argv[2]);
	if (a < 0)
		a += n;
	if (b < 0)
		b += n;
	a = a < 0 ? 0 : a > n ? n : a;
	b = b < a ? a : b > n ? n : b;
	if (argv[0].type == VAL_STRING) {
		*ret = mp_string(argv[0].as.s + a, (int)(b - a));
		return true;
	}
	*ret = mp_list();
	for (int64_t i = a; i < b; i++)
		mp_push(*ret, value_copy(argv[0].as.list->items[i]));
	return true;
}

static int script_argc;
static char **script_argv;

void builtin_set_args(int argc, char **argv)
{
	script_argc = argc;
	script_argv = argv;
}

static bool b_args(int argc, Value *argv, Value *ret)
{
	(void)argv;
	if (argc != 0)
		return mp_error(ret, "args() takes no arguments.");
	*ret = mp_list();
	for (int i = 0; i < script_argc; i++)
		mp_push(*ret, mp_str(script_argv[i]));
	return true;
}

static const MpEntry builtins[] = {
	{"args", b_args},
	{"len", b_len}, {"push", b_push}, {"pop", b_pop}, {"keys", b_keys},
	{"values", b_values}, {"has", b_has}, {"get", b_get}, {"remove", b_remove},
	{"type", b_type}, {"text", b_text}, {"int", b_int}, {"float", b_float},
	{"range", b_range}, {"sort", b_sort}, {"slice", b_slice},
	{NULL, NULL}
};

void builtin_install(Table *globals)
{
	for (const MpEntry *e = builtins; e->name; e++) {
		Value v;
		v.type = VAL_NATIVE;
		v.as.native = e;
		table_set(globals, e->name, (int)strlen(e->name), v);
	}
}
