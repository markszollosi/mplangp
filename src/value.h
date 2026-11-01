#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	VAL_NONE,
	VAL_BOOL,
	VAL_INT,
	VAL_FLOAT,
	VAL_STRING,
	VAL_FUNC,
	VAL_NATIVE,
	VAL_MODULE,
	VAL_LIST,
	VAL_MAP
} ValueType;

struct Function;
struct MpEntry;
struct Module;
struct List;
struct Map;

typedef struct Value {
	ValueType type;
	union {
		bool b;
		int64_t i;
		double f;
		char *s;
		struct Function *fn;        /* refcounted */
		const struct MpEntry *native; /* borrowed, owned by its module */
		struct Module *module;      /* borrowed, owned by the module registry */
		struct List *list;          /* refcounted, shared by reference */
		struct Map *map;            /* refcounted, string keys in insertion order */
	} as;
} Value;

typedef struct List {
	int refs;
	int count, cap;
	Value *items;
} List;

typedef struct Map {
	int refs;
	int count, cap;
	char **keys;
	Value *vals;
} Map;

Value value_none(void);
Value value_bool(bool b);
Value value_int(int64_t i);
Value value_float(double f);
Value value_string(const char *chars, int length);
Value value_copy(Value v);
void value_free(Value v);
bool value_is_truthy(Value v);
bool value_equals(Value a, Value b);
void value_print(Value v);
void value_map_put(Value map, const char *key, Value val); /* takes val */
Value value_to_string(Value v);   /* text form; strings come back unquoted */

typedef struct {
	Value *values;
	int count;
	int capacity;
} ValueArray;

void value_array_init(ValueArray *a);
void value_array_write(ValueArray *a, Value v);
void value_array_free(ValueArray *a);

#endif
