#include "mplangpfunc.h"

typedef struct {
	char *s;
	size_t n, cap;
} Buf;

static void add(Buf *b, char c)
{
	if (b->n + 2 > b->cap) {
		size_t cap = b->cap ? b->cap * 2 : 64;
		char *s = mp_alloc(cap);
		if (b->n)
			memcpy(s, b->s, b->n);
		free(b->s);
		b->s = s;
		b->cap = cap;
	}
	b->s[b->n++] = c;
	b->s[b->n] = '\0';
}

static bool sep_arg(int argc, Value *argv, int at, char *sep, Value *ret)
{
	*sep = ',';
	if (argc > at) {
		if (!mp_is_str(argv[at]) || strlen(argv[at].as.s) != 1)
			return mp_error(ret, "the separator must be a one-character string.");
		*sep = argv[at].as.s[0];
	}
	return true;
}

static bool c_parse(int argc, Value *argv, Value *ret)
{
	char sep;
	MP_CHECK(argc >= 1 && argc <= 2 && mp_is_str(argv[0]), "parse(text, sep) takes a string and an optional separator.");
	if (!sep_arg(argc, argv, 1, &sep, ret))
		return false;
	const char *p = argv[0].as.s;
	Value rows = mp_list(), row = mp_list();
	Buf f = { NULL, 0, 0 };
	bool any = false, quoted = false;
	while (*p) {
		char c = *p++;
		any = true;
		if (quoted) {
			if (c == '"' && *p == '"') {
				add(&f, '"');
				p++;
			} else if (c == '"') {
				quoted = false;
			} else {
				add(&f, c);
			}
		} else if (c == '"') {
			quoted = true;
		} else if (c == sep) {
			mp_push(row, mp_str(f.s ? f.s : ""));
			f.n = 0;
			if (f.s)
				f.s[0] = '\0';
		} else if (c == '\n' || c == '\r') {
			if (c == '\r' && *p == '\n')
				p++;
			mp_push(row, mp_str(f.s ? f.s : ""));
			mp_push(rows, row);
			row = mp_list();
			f.n = 0;
			if (f.s)
				f.s[0] = '\0';
			any = false;
		} else {
			add(&f, c);
		}
	}
	if (quoted) {
		free(f.s);
		mp_value_release(row);
		mp_value_release(rows);
		return mp_error(ret, "parse(text): unterminated quote.");
	}
	if (any) {
		mp_push(row, mp_str(f.s ? f.s : ""));
		mp_push(rows, row);
	} else {
		mp_value_release(row);
	}
	free(f.s);
	*ret = rows;
	return true;
}

static bool c_dump(int argc, Value *argv, Value *ret)
{
	char sep;
	MP_CHECK(argc >= 1 && argc <= 2 && argv[0].type == VAL_LIST, "dump(rows, sep) takes a list of lists and an optional separator.");
	if (!sep_arg(argc, argv, 1, &sep, ret))
		return false;
	Buf b = { NULL, 0, 0 };
	for (int r = 0; r < argv[0].as.list->count; r++) {
		Value row = argv[0].as.list->items[r];
		if (row.type != VAL_LIST) {
			free(b.s);
			return mp_error(ret, "dump(rows) needs a list of lists.");
		}
		for (int i = 0; i < row.as.list->count; i++) {
			Value v = row.as.list->items[i];
			char num[64];
			const char *t;
			if (mp_is_str(v)) {
				t = v.as.s;
			} else if (v.type == VAL_INT) {
				snprintf(num, sizeof num, "%lld", (long long)v.as.i);
				t = num;
			} else if (v.type == VAL_FLOAT) {
				snprintf(num, sizeof num, "%.12g", v.as.f);
				t = num;
			} else if (v.type == VAL_BOOL) {
				t = v.as.b ? "True" : "False";
			} else if (v.type == VAL_NONE) {
				t = "";
			} else {
				free(b.s);
				return mp_error(ret, "dump(rows): cells must be strings, numbers, bools or none.");
			}
			if (i)
				add(&b, sep);
			bool q = strpbrk(t, "\"\r\n") != NULL || strchr(t, sep) != NULL;
			if (q)
				add(&b, '"');
			for (; *t; t++) {
				if (*t == '"')
					add(&b, '"');
				add(&b, *t);
			}
			if (q)
				add(&b, '"');
		}
		add(&b, '\n');
	}
	*ret = mp_str(b.s ? b.s : "");
	free(b.s);
	return true;
}

MP_MODULE({"parse", c_parse}, {"dump", c_dump});
