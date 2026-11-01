#include <stdint.h>
#include <stdio.h>
#include "mplangpfunc.h"

static bool seeded;
static uint64_t state;

static uint64_t next(void)
{
	if (seeded) {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return state;
	}
	uint64_t v = 0;
	FILE *f = fopen("/dev/urandom", "rb");
	if (!f || fread(&v, sizeof v, 1, f) != 1)
		v = (uint64_t)rand() * 2654435761u;
	if (f)
		fclose(f);
	return v;
}

static bool r_seed(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_int(argv[0]), "seed(n) takes an integer.");
	state = (uint64_t)mp_to_int(argv[0]) * 0x9E3779B97F4A7C15ull + 0x1234567ull;
	if (!state)
		state = 1;
	seeded = true;
	return true;
}

static bool r_int(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_int(argv[0]) && mp_is_int(argv[1]), "int(lo, hi) takes two integers.");
	int64_t lo = mp_to_int(argv[0]), hi = mp_to_int(argv[1]);
	MP_CHECK(lo <= hi, "int(lo, hi) needs lo <= hi.");
	uint64_t span = (uint64_t)hi - (uint64_t)lo + 1;
	*ret = mp_int(span == 0 ? (int64_t)next() : (int64_t)((uint64_t)lo + next() % span));
	return true;
}

static bool r_float(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "float() takes no arguments.");
	*ret = mp_float((double)(next() >> 11) / 9007199254740992.0);
	return true;
}

static bool r_choice(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && argv[0].type == VAL_LIST, "choice(list) takes a list.");
	MP_CHECK(argv[0].as.list->count > 0, "choice(list) needs a non-empty list.");
	Value v = argv[0].as.list->items[next() % (uint64_t)argv[0].as.list->count];
	switch (v.type) {
	case VAL_STRING: *ret = mp_str(v.as.s); break;
	case VAL_NONE: case VAL_BOOL: case VAL_INT: case VAL_FLOAT: *ret = v; break;
	default: return mp_error(ret, "choice(list) only picks plain values.");
	}
	return true;
}

static bool r_shuffle(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && argv[0].type == VAL_LIST, "shuffle(list) takes a list.");
	List *l = argv[0].as.list;
	for (int i = l->count - 1; i > 0; i--) {
		int j = (int)(next() % (uint64_t)(i + 1));
		Value t = l->items[i];
		l->items[i] = l->items[j];
		l->items[j] = t;
	}
	return true;
}

static bool r_hex(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_int(argv[0]), "hex(nbytes) takes an integer.");
	int64_t n = mp_to_int(argv[0]);
	MP_CHECK(n >= 1 && n <= 4096, "hex(nbytes) takes a number from 1 to 4096.");
	char *out = mp_alloc((size_t)n * 2 + 1);
	for (int64_t i = 0; i < n; i++)
		sprintf(out + i * 2, "%02x", (unsigned)(next() & 0xFF));
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool r_uuid(int argc, Value *argv, Value *ret)
{
	char buf[40];
	(void)argv;
	MP_CHECK(argc == 0, "uuid() takes no arguments.");
	uint64_t a = next(), b = next();
	a = (a & 0xFFFFFFFFFFFF0FFFull) | 0x4000ull;
	b = (b & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;
	snprintf(buf, sizeof buf, "%08x-%04x-%04x-%04x-%012llx", (unsigned)(a >> 32), (unsigned)((a >> 16) & 0xFFFF),
	    (unsigned)(a & 0xFFFF), (unsigned)(b >> 48), (unsigned long long)(b & 0xFFFFFFFFFFFFull));
	*ret = mp_str(buf);
	return true;
}

MP_MODULE({"seed", r_seed}, {"int", r_int}, {"float", r_float}, {"choice", r_choice},
	{"shuffle", r_shuffle}, {"hex", r_hex}, {"uuid", r_uuid});
