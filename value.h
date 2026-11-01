#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { VAL_NONE, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING } ValueType;

typedef struct {
	ValueType type;
	union { bool b; int64_t i; double f; char *s; } as;
} Value;

Value value_none(void);
Value value_bool(bool b);
Value value_int(int64_t i);
Value value_float(double f);
Value value_string(const char *chars, int length);
void value_free(Value v);
bool value_is_truthy(Value v);
bool value_equals(Value a, Value b);
void value_print(Value v);

typedef struct {
	Value *values;
	int count, capacity;
} ValueArray;

void value_array_init(ValueArray *a);
void value_array_write(ValueArray *a, Value v);
void value_array_free(ValueArray *a);

#endif
