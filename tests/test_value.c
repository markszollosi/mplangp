#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "value.h"
#include "mplangpfunc.h"

static char capture_buf[256];

static const char *capture_print(Value v)
{
	FILE *f = tmpfile();
	FILE *saved_stdout = stdout;
	stdout = f;
	value_print(v);
	fflush(f);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	size_t n = fread(capture_buf, 1, sizeof(capture_buf) - 1, f);
	(void)len;
	capture_buf[n] = '\0';
	stdout = saved_stdout;
	fclose(f);
	return capture_buf;
}

static void test_construction(void)
{
	Value n = value_none();
	assert(n.type == VAL_NONE);

	Value bt = value_bool(true);
	assert(bt.type == VAL_BOOL && bt.as.b == true);

	Value bf = value_bool(false);
	assert(bf.type == VAL_BOOL && bf.as.b == false);

	Value i = value_int(42);
	assert(i.type == VAL_INT && i.as.i == 42);

	Value f = value_float(3.5);
	assert(f.type == VAL_FLOAT && f.as.f == 3.5);

	Value s = value_string("hello", 5);
	assert(s.type == VAL_STRING);
	assert(strcmp(s.as.s, "hello") == 0);
	value_free(s);

	Value empty = value_string("", 0);
	assert(empty.type == VAL_STRING);
	assert(strcmp(empty.as.s, "") == 0);
	value_free(empty);
}

static void test_truthy(void)
{
	assert(value_is_truthy(value_none()) == false);
	assert(value_is_truthy(value_bool(true)) == true);
	assert(value_is_truthy(value_bool(false)) == false);
	assert(value_is_truthy(value_int(0)) == false);
	assert(value_is_truthy(value_int(1)) == true);
	assert(value_is_truthy(value_int(-1)) == true);
	assert(value_is_truthy(value_float(0.0)) == false);
	assert(value_is_truthy(value_float(0.5)) == true);

	Value empty = value_string("", 0);
	assert(value_is_truthy(empty) == false);
	value_free(empty);

	Value nonempty = value_string("x", 1);
	assert(value_is_truthy(nonempty) == true);
	value_free(nonempty);
}

static void test_equals(void)
{
	assert(value_equals(value_none(), value_none()) == true);
	assert(value_equals(value_bool(true), value_bool(true)) == true);
	assert(value_equals(value_bool(true), value_bool(false)) == false);
	assert(value_equals(value_int(5), value_int(5)) == true);
	assert(value_equals(value_int(5), value_int(6)) == false);
	assert(value_equals(value_float(1.5), value_float(1.5)) == true);
	assert(value_equals(value_float(1.5), value_float(2.5)) == false);

	assert(value_equals(value_int(0), value_none()) == false);
	assert(value_equals(value_int(1), value_bool(true)) == false);

	Value a = value_string("abc", 3);
	Value b = value_string("abc", 3);
	Value c = value_string("abd", 3);
	assert(value_equals(a, b) == true);
	assert(value_equals(a, c) == false);
	value_free(a);
	value_free(b);
	value_free(c);
}

static void test_print(void)
{
	assert(strcmp(capture_print(value_none()), "None") == 0);
	assert(strcmp(capture_print(value_bool(true)), "True") == 0);
	assert(strcmp(capture_print(value_bool(false)), "False") == 0);
	assert(strcmp(capture_print(value_int(42)), "42") == 0);
	assert(strcmp(capture_print(value_int(-7)), "-7") == 0);
	assert(strcmp(capture_print(value_float(3.0)), "3.0") == 0);
	assert(strcmp(capture_print(value_float(3.5)), "3.5") == 0);

	Value s = value_string("hello", 5);
	assert(strcmp(capture_print(s), "hello") == 0);
	value_free(s);
}

static void test_array(void)
{
	ValueArray arr;
	value_array_init(&arr);
	assert(arr.count == 0);

	for (int i = 0; i < 20; i++)
		value_array_write(&arr, value_int(i));

	assert(arr.count == 20);
	for (int i = 0; i < 20; i++)
		assert(arr.values[i].as.i == i);

	value_array_free(&arr);
	assert(arr.count == 0);
	assert(arr.capacity == 0);
	assert(arr.values == NULL);
}

static void test_collections(void)
{
	Value l = mp_list(), m = mp_map();
	mp_push(l, value_int(1));
	mp_push(l, value_string("a", 1));
	mp_set(m, "k", value_copy(l));
	mp_set(m, "n", value_none());
	assert(l.as.list->refs == 2);
	assert(m.as.map->count == 2);
	assert(mp_get(m, "k")->type == VAL_LIST);
	assert(mp_get(m, "zz") == NULL);
	assert(value_is_truthy(l) && value_is_truthy(m));
	assert(strcmp(capture_print(l), "[1, \"a\"]") == 0);
	assert(strcmp(capture_print(m), "{\"k\": [1, \"a\"], \"n\": None}") == 0);

	Value l2 = mp_list();
	mp_push(l2, value_int(1));
	mp_push(l2, value_string("a", 1));
	assert(value_equals(l, l2));
	mp_push(l2, value_int(3));
	assert(!value_equals(l, l2));

	value_map_put(m, "n", value_int(5));
	assert(m.as.map->count == 2 && mp_get(m, "n")->as.i == 5);

	Value c = value_copy(m);
	assert(m.as.map->refs == 2);
	value_free(c);
	value_free(m);
	assert(l.as.list->refs == 1);
	value_free(l);
	value_free(l2);
}

int main(void)
{
	test_construction();
	test_truthy();
	test_equals();
	test_print();
	test_array();
	test_collections();
	printf("all tests passed\n");
	return 0;
}
