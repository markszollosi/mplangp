#ifndef MPLANGPFUNC_H
#define MPLANGPFUNC_H

/*
 * API for native mplangp modules. A module is one C file that includes this
 * header, defines functions of type MpFunc and lists them with MP_MODULE.
 * It is built as a shared library named modules/<name>.so and loaded by
 * `import <name>`. Nothing here needs symbols from the interpreter.
 *
 * Lists and maps are refcounted and shared by reference. A module builds them
 * with mp_list / mp_push and mp_map / mp_set (which take ownership of the
 * item) and returns them; it reads arguments through mp_get and the count/items
 * fields. Arguments are borrowed: never free or keep them. Store the result in *ret
 * and return true. To raise a runtime error, return mp_error(ret, "message").
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "value.h"

#define MP_ABI 2

typedef bool (*MpFunc)(int argc, Value *argv, Value *ret);

typedef struct MpEntry {
	const char *name;
	MpFunc fn;
} MpEntry;

#define MP_MODULE(...) \
	const int mplangp_abi = MP_ABI; \
	const MpEntry mplangp_entries[] = { __VA_ARGS__, { NULL, NULL } }

static inline Value mp_none(void)
{
	Value v;
	v.type = VAL_NONE;
	return v;
}

static inline Value mp_bool(bool b)
{
	Value v;
	v.type = VAL_BOOL;
	v.as.b = b;
	return v;
}

static inline Value mp_int(int64_t i)
{
	Value v;
	v.type = VAL_INT;
	v.as.i = i;
	return v;
}

static inline Value mp_float(double f)
{
	Value v;
	v.type = VAL_FLOAT;
	v.as.f = f;
	return v;
}

static inline Value mp_string(const char *chars, int length)
{
	Value v;
	v.type = VAL_STRING;
	v.as.s = malloc((size_t)length + 1);
	if (v.as.s) {
		memcpy(v.as.s, chars, (size_t)length);
		v.as.s[length] = '\0';
	}
	return v;
}

static inline void *mp_alloc(size_t n)
{
	void *p = malloc(n ? n : 1);
	if (!p)
		abort();
	return p;
}

static inline Value mp_list(void)
{
	Value v;
	v.type = VAL_LIST;
	v.as.list = mp_alloc(sizeof *v.as.list);
	v.as.list->refs = 1;
	v.as.list->count = v.as.list->cap = 0;
	v.as.list->items = NULL;
	return v;
}

static inline void mp_push(Value list, Value item)
{
	List *l = list.as.list;
	if (l->count == l->cap) {
		int cap = l->cap ? l->cap * 2 : 8;
		Value *items = mp_alloc(sizeof *items * (size_t)cap);
		if (l->count)
			memcpy(items, l->items, sizeof *items * (size_t)l->count);
		free(l->items);
		l->items = items;
		l->cap = cap;
	}
	l->items[l->count++] = item;
}

static inline Value mp_map(void)
{
	Value v;
	v.type = VAL_MAP;
	v.as.map = mp_alloc(sizeof *v.as.map);
	v.as.map->refs = 1;
	v.as.map->count = v.as.map->cap = 0;
	v.as.map->keys = NULL;
	v.as.map->vals = NULL;
	return v;
}

static inline int mp_find(Value map, const char *key)
{
	Map *m = map.as.map;
	for (int i = 0; i < m->count; i++)
		if (strcmp(m->keys[i], key) == 0)
			return i;
	return -1;
}

static inline Value *mp_get(Value map, const char *key)
{
	int i = mp_find(map, key);
	return i < 0 ? NULL : &map.as.map->vals[i];
}

static inline void mp_value_release(Value v);

/* Takes ownership of val; replaces an existing key. */
static inline void mp_set(Value map, const char *key, Value val)
{
	Map *m = map.as.map;
	int i = mp_find(map, key);
	if (i >= 0) {
		mp_value_release(m->vals[i]);
		m->vals[i] = val;
		return;
	}
	if (m->count == m->cap) {
		int cap = m->cap ? m->cap * 2 : 8;
		char **keys = mp_alloc(sizeof *keys * (size_t)cap);
		Value *vals = mp_alloc(sizeof *vals * (size_t)cap);
		if (m->count) {
			memcpy(keys, m->keys, sizeof *keys * (size_t)m->count);
			memcpy(vals, m->vals, sizeof *vals * (size_t)m->count);
		}
		free(m->keys);
		free(m->vals);
		m->keys = keys;
		m->vals = vals;
		m->cap = cap;
	}
	size_t n = strlen(key) + 1;
	m->keys[m->count] = mp_alloc(n);
	memcpy(m->keys[m->count], key, n);
	m->vals[m->count++] = val;
}

/* Frees a value a module built itself (no functions inside). */
static inline void mp_value_release(Value v)
{
	if (v.type == VAL_STRING) {
		free(v.as.s);
	} else if (v.type == VAL_LIST && --v.as.list->refs == 0) {
		for (int i = 0; i < v.as.list->count; i++)
			mp_value_release(v.as.list->items[i]);
		free(v.as.list->items);
		free(v.as.list);
	} else if (v.type == VAL_MAP && --v.as.map->refs == 0) {
		for (int i = 0; i < v.as.map->count; i++) {
			free(v.as.map->keys[i]);
			mp_value_release(v.as.map->vals[i]);
		}
		free(v.as.map->keys);
		free(v.as.map->vals);
		free(v.as.map);
	}
}

static inline bool mp_is_str(Value v)
{
	return v.type == VAL_STRING;
}

/* Returns the string stored under key in a map, or NULL. */
static inline const char *mp_get_str(Value map, const char *key)
{
	Value *v = mp_get(map, key);
	return v && v->type == VAL_STRING ? v->as.s : NULL;
}

static inline bool mp_is_int(Value v)
{
	return v.type == VAL_INT || v.type == VAL_BOOL;
}

static inline bool mp_is_num(Value v)
{
	return mp_is_int(v) || v.type == VAL_FLOAT;
}

static inline int64_t mp_to_int(Value v)
{
	return v.type == VAL_BOOL ? v.as.b : v.as.i;
}

static inline double mp_to_float(Value v)
{
	return v.type == VAL_FLOAT ? v.as.f : (double)mp_to_int(v);
}

static inline bool mp_error(Value *ret, const char *msg)
{
	*ret = mp_string(msg, (int)strlen(msg));
	return false;
}

static inline bool mp_errorf(Value *ret, const char *fmt, ...)
{
	char buf[240];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	return mp_error(ret, buf);
}

#define MP_CHECK(cond, msg) do { if (!(cond)) return mp_error(ret, msg); } while (0)

/* Strings cannot hold NUL bytes: external data gets them dropped. */
static inline Value mp_string_clean(const char *chars, size_t len)
{
	Value v = mp_string("", 0);
	char *out = mp_alloc(len + 1);
	size_t n = 0;
	for (size_t i = 0; i < len; i++)
		if (chars[i])
			out[n++] = chars[i];
	out[n] = '\0';
	free(v.as.s);
	v.as.s = out;
	return v;
}

static inline Value mp_str(const char *s)
{
	return mp_string(s, (int)strlen(s));
}

#endif
