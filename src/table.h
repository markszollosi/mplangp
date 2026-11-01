#ifndef TABLE_H
#define TABLE_H

#include <stdbool.h>
#include "value.h"

typedef struct Table Table;

Table *table_new(void);
void table_free(Table *t);
bool table_get(Table *t, const char *key, int length, Value *out);
void table_set(Table *t, const char *key, int length, Value v);

#endif
