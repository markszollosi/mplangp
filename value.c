#include "value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void value_free(Value v)
{
	if (v.type == VAL_STRING)
		free(v.as.s);
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
	}
	return false;
}

bool value_equals(Value a, Value b)
{
	if (a.type != b.type)
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
	}
	return false;
}

void value_print(Value v)
{
	switch (v.type) {
	case VAL_NONE:
		printf("None");
		break;
	case VAL_BOOL:
		printf("%s", v.as.b ? "True" : "False");
		break;
	case VAL_INT:
		printf("%lld", (long long)v.as.i);
		break;
	case VAL_FLOAT: {
		char buf[32];
		snprintf(buf, sizeof(buf), "%.12g", v.as.f);
		if (strpbrk(buf, ".eEnN") == NULL)
			strncat(buf, ".0", sizeof(buf) - strlen(buf) - 1);
		printf("%s", buf);
		break;
	}
	case VAL_STRING:
		printf("%s", v.as.s);
		break;
	}
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
	free(a->values);
	value_array_init(a);
}
