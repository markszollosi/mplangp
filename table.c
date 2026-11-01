#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "util.h"

typedef struct {
	char *key;
	int length;
	uint32_t hash;
	Value value;
} Entry;

struct Table {
	Entry *entries;
	int count, capacity;
};

static uint32_t hash_key(const char *key, int length)
{
	uint32_t h = 2166136261u;
	for (int i = 0; i < length; i++) {
		h ^= (unsigned char)key[i];
		h *= 16777619u;
	}
	return h;
}

static Entry *find(Entry *entries, int capacity, const char *key, int length, uint32_t hash)
{
	uint32_t i = hash & (capacity - 1);
	for (;;) {
		Entry *e = &entries[i];
		if (e->key == NULL || (e->hash == hash && e->length == length && memcmp(e->key, key, length) == 0)) {
			return e;
		}
		i = (i + 1) & (capacity - 1);
	}
}

static void grow(Table *t)
{
	int capacity = t->capacity ? t->capacity * 2 : 8;
	Entry *entries = xmalloc(sizeof(Entry) * capacity);
	for (int i = 0; i < capacity; i++) {
		entries[i].key = NULL;
	}
	for (int i = 0; i < t->capacity; i++) {
		Entry *e = &t->entries[i];
		if (e->key != NULL) {
			*find(entries, capacity, e->key, e->length, e->hash) = *e;
		}
	}
	free(t->entries);
	t->entries = entries;
	t->capacity = capacity;
}

Table *table_new(void)
{
	Table *t = xmalloc(sizeof(Table));
	t->entries = NULL;
	t->count = t->capacity = 0;
	return t;
}

void table_free(Table *t)
{
	for (int i = 0; i < t->capacity; i++) {
		if (t->entries[i].key != NULL) {
			free(t->entries[i].key);
			value_free(t->entries[i].value);
		}
	}
	free(t->entries);
	free(t);
}

bool table_get(Table *t, const char *key, int length, Value *out)
{
	if (t->count == 0) {
		return false;
	}
	Entry *e = find(t->entries, t->capacity, key, length, hash_key(key, length));
	if (e->key == NULL) {
		return false;
	}
	*out = e->value;
	return true;
}

void table_set(Table *t, const char *key, int length, Value v)
{
	if ((t->count + 1) * 4 > t->capacity * 3) {
		grow(t);
	}
	uint32_t hash = hash_key(key, length);
	Entry *e = find(t->entries, t->capacity, key, length, hash);
	if (e->key != NULL) {
		value_free(e->value);
		e->value = v;
		return;
	}
	e->key = xmalloc(length + 1);
	memcpy(e->key, key, length);
	e->key[length] = '\0';
	e->length = length;
	e->hash = hash;
	e->value = v;
	t->count++;
}
