#ifndef MPLANGPFUNC_H
#define MPLANGPFUNC_H

/*
 * API for native mplangp modules. A module is one C file that includes this
 * header, defines functions of type MpFunc and lists them with MP_MODULE.
 * It is built as a shared library named modules/<name>.so and loaded by
 * `import <name>`. Nothing here needs symbols from the interpreter.
 *
 * Arguments are borrowed: never free or keep them. Store the result in *ret
 * and return true. To raise a runtime error, return mp_error(ret, "message").
 */

#include <stdlib.h>
#include <string.h>
#include "value.h"

#define MP_ABI 1

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

#endif
