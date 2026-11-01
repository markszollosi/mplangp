#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"
#include "util.h"

static uint32_t fnv(const char *s, int n)
{
	uint32_t h = 2166136261u;
	for (int i = 0; i < n; i++) {
		h ^= (unsigned char)s[i];
		h *= 16777619u;
	}
	return h;
}

static int cmp(const void *a, const void *b)
{
	uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
	return x < y ? -1 : x > y;
}

static void test_many(void)
{
	Table *t = table_new();
	char k[32];
	Value v;
	for (int i = 0; i < 10000; i++) {
		int n = sprintf(k, "var%d", i);
		table_set(t, k, n, value_int(i));
	}
	for (int i = 0; i < 10000; i++) {
		int n = sprintf(k, "var%d", i);
		assert(table_get(t, k, n, &v) && v.type == VAL_INT && v.as.i == i);
	}
	assert(!table_get(t, "var10000", 8, &v));
	assert(!table_get(t, "nope", 4, &v));
	for (int i = 0; i < 10000; i += 2) {
		int n = sprintf(k, "var%d", i);
		table_set(t, k, n, value_string("hello", 5));
	}
	for (int i = 0; i < 10000; i++) {
		int n = sprintf(k, "var%d", i);
		assert(table_get(t, k, n, &v));
		if (i % 2) {
			assert(v.type == VAL_INT && v.as.i == i);
		} else {
			assert(v.type == VAL_STRING && strcmp(v.as.s, "hello") == 0);
		}
	}
	table_free(t);
}

static void test_empty_and_prefix(void)
{
	Table *t = table_new();
	Value v;
	assert(!table_get(t, "a", 1, &v));
	table_set(t, "ab", 2, value_int(1));
	table_set(t, "abc", 3, value_int(2));
	table_set(t, "a", 1, value_int(3));
	assert(table_get(t, "ab", 2, &v) && v.as.i == 1);
	assert(table_get(t, "abc", 3, &v) && v.as.i == 2);
	assert(table_get(t, "a", 1, &v) && v.as.i == 3);
	assert(!table_get(t, "abcd", 4, &v));
	assert(table_get(t, "abcd", 3, &v) && v.as.i == 2);
	table_set(t, "x", 1, value_none());
	assert(table_get(t, "x", 1, &v) && v.type == VAL_NONE);
	table_free(t);
}

static void test_collision(void)
{
	enum { N = 400000 };
	uint64_t *h = xmalloc(sizeof(uint64_t) * N);
	char k[32];
	for (int i = 0; i < N; i++) {
		int n = sprintf(k, "c%x%x", (unsigned)(i * 2654435761u), (unsigned)i);
		h[i] = ((uint64_t)fnv(k, n) << 32) | (uint32_t)i;
	}
	qsort(h, N, sizeof(uint64_t), cmp);
	int a = -1, b = -1;
	for (int i = 1; i < N; i++) {
		if ((h[i] >> 32) == (h[i - 1] >> 32)) {
			a = (int)(uint32_t)h[i - 1];
			b = (int)(uint32_t)h[i];
			break;
		}
	}
	free(h);
	assert(a >= 0);
	char ka[32], kb[32];
	int na = sprintf(ka, "c%x%x", (unsigned)(a * 2654435761u), (unsigned)a), nb = sprintf(kb, "c%x%x", (unsigned)(b * 2654435761u), (unsigned)b);
	assert(fnv(ka, na) == fnv(kb, nb));
	Table *t = table_new();
	Value v;
	table_set(t, ka, na, value_int(1));
	table_set(t, kb, nb, value_int(2));
	assert(table_get(t, ka, na, &v) && v.as.i == 1);
	assert(table_get(t, kb, nb, &v) && v.as.i == 2);
	table_set(t, ka, na, value_int(3));
	assert(table_get(t, ka, na, &v) && v.as.i == 3);
	assert(table_get(t, kb, nb, &v) && v.as.i == 2);
	table_free(t);
}

int main(void)
{
	test_many();
	test_empty_and_prefix();
	test_collision();
	puts("table ok");
	return 0;
}
