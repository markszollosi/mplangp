#include <ctype.h>
#include <errno.h>
#include <math.h>
#include "mplangpfunc.h"

#define DEPTH_MAX 200

typedef struct {
	char *s;
	size_t n, cap;
} Buf;

static void add(Buf *b, const char *chars, size_t n)
{
	if (b->n + n + 1 > b->cap) {
		size_t cap = (b->n + n + 1) * 2;
		char *s = mp_alloc(cap);
		if (b->n)
			memcpy(s, b->s, b->n);
		free(b->s);
		b->s = s;
		b->cap = cap;
	}
	memcpy(b->s + b->n, chars, n);
	b->n += n;
	b->s[b->n] = '\0';
}

static void adds(Buf *b, const char *s)
{
	add(b, s, strlen(s));
}

typedef struct {
	const char *p;
	char err[96];
} Parser;

static void fail(Parser *ps, const char *msg)
{
	if (!ps->err[0])
		snprintf(ps->err, sizeof ps->err, "invalid JSON: %s", msg);
}

static void skip(Parser *ps)
{
	while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r')
		ps->p++;
}

static int hex4(const char *p)
{
	int v = 0;
	for (int i = 0; i < 4; i++) {
		if (!isxdigit((unsigned char)p[i]))
			return -1;
		v = v * 16 + (isdigit((unsigned char)p[i]) ? p[i] - '0' : (tolower((unsigned char)p[i]) - 'a' + 10));
	}
	return v;
}

static void utf8(Buf *b, unsigned long cp)
{
	char o[4];
	size_t n;
	if (cp < 0x80) {
		o[0] = (char)cp;
		n = 1;
	} else if (cp < 0x800) {
		o[0] = (char)(0xC0 | (cp >> 6));
		o[1] = (char)(0x80 | (cp & 0x3F));
		n = 2;
	} else if (cp < 0x10000) {
		o[0] = (char)(0xE0 | (cp >> 12));
		o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
		o[2] = (char)(0x80 | (cp & 0x3F));
		n = 3;
	} else {
		o[0] = (char)(0xF0 | (cp >> 18));
		o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
		o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
		o[3] = (char)(0x80 | (cp & 0x3F));
		n = 4;
	}
	add(b, o, n);
}

/* Parses a string literal; *out is a malloc'd C string or NULL on error. */
static char *parse_string(Parser *ps)
{
	Buf b = { NULL, 0, 0 };
	ps->p++;
	for (;;) {
		char c = *ps->p;
		if (c == '\0' || (unsigned char)c < 0x20) {
			fail(ps, "unterminated string");
			free(b.s);
			return NULL;
		}
		ps->p++;
		if (c == '"')
			break;
		if (c != '\\') {
			add(&b, &c, 1);
			continue;
		}
		c = *ps->p++;
		switch (c) {
		case '"': case '\\': case '/': add(&b, &c, 1); break;
		case 'b': add(&b, "\b", 1); break;
		case 'f': add(&b, "\f", 1); break;
		case 'n': add(&b, "\n", 1); break;
		case 'r': add(&b, "\r", 1); break;
		case 't': add(&b, "\t", 1); break;
		case 'u': {
			int cp = hex4(ps->p);
			if (cp < 0) {
				fail(ps, "bad \\u escape");
				free(b.s);
				return NULL;
			}
			ps->p += 4;
			if (cp >= 0xD800 && cp < 0xDC00 && ps->p[0] == '\\' && ps->p[1] == 'u') {
				int lo = hex4(ps->p + 2);
				if (lo >= 0xDC00 && lo < 0xE000) {
					cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
					ps->p += 6;
				}
			}
			if (cp == 0)
				cp = '?';
			utf8(&b, (unsigned long)cp);
			break;
		}
		default:
			fail(ps, "bad escape");
			free(b.s);
			return NULL;
		}
	}
	if (!b.s)
		add(&b, "", 0);
	return b.s;
}

static bool parse_value(Parser *ps, Value *out, int depth);

static bool parse_number(Parser *ps, Value *out)
{
	const char *start = ps->p;
	bool is_float = false;
	if (*ps->p == '-')
		ps->p++;
	if (!isdigit((unsigned char)*ps->p)) {
		fail(ps, "bad number");
		return false;
	}
	while (isdigit((unsigned char)*ps->p))
		ps->p++;
	if (*ps->p == '.') {
		is_float = true;
		ps->p++;
		while (isdigit((unsigned char)*ps->p))
			ps->p++;
	}
	if (*ps->p == 'e' || *ps->p == 'E') {
		is_float = true;
		ps->p++;
		if (*ps->p == '+' || *ps->p == '-')
			ps->p++;
		while (isdigit((unsigned char)*ps->p))
			ps->p++;
	}
	char buf[64];
	size_t n = (size_t)(ps->p - start);
	if (n >= sizeof buf) {
		fail(ps, "number too long");
		return false;
	}
	memcpy(buf, start, n);
	buf[n] = '\0';
	if (!is_float) {
		errno = 0;
		long long v = strtoll(buf, NULL, 10);
		if (errno == 0) {
			*out = mp_int(v);
			return true;
		}
	}
	*out = mp_float(strtod(buf, NULL));
	return true;
}

static bool parse_value(Parser *ps, Value *out, int depth)
{
	skip(ps);
	if (depth > DEPTH_MAX) {
		fail(ps, "nesting too deep");
		return false;
	}
	char c = *ps->p;
	if (c == '"') {
		char *s = parse_string(ps);
		if (!s)
			return false;
		*out = mp_str(s);
		free(s);
		return true;
	}
	if (c == '-' || isdigit((unsigned char)c))
		return parse_number(ps, out);
	if (c == '[') {
		ps->p++;
		Value list = mp_list();
		skip(ps);
		if (*ps->p == ']') {
			ps->p++;
			*out = list;
			return true;
		}
		for (;;) {
			Value item;
			if (!parse_value(ps, &item, depth + 1)) {
				mp_value_release(list);
				return false;
			}
			mp_push(list, item);
			skip(ps);
			if (*ps->p == ',') {
				ps->p++;
				continue;
			}
			if (*ps->p == ']') {
				ps->p++;
				*out = list;
				return true;
			}
			fail(ps, "expected ',' or ']'");
			mp_value_release(list);
			return false;
		}
	}
	if (c == '{') {
		ps->p++;
		Value map = mp_map();
		skip(ps);
		if (*ps->p == '}') {
			ps->p++;
			*out = map;
			return true;
		}
		for (;;) {
			skip(ps);
			if (*ps->p != '"') {
				fail(ps, "expected a string key");
				mp_value_release(map);
				return false;
			}
			char *key = parse_string(ps);
			if (!key) {
				mp_value_release(map);
				return false;
			}
			skip(ps);
			if (*ps->p != ':') {
				fail(ps, "expected ':'");
				free(key);
				mp_value_release(map);
				return false;
			}
			ps->p++;
			Value item;
			if (!parse_value(ps, &item, depth + 1)) {
				free(key);
				mp_value_release(map);
				return false;
			}
			mp_set(map, key, item);
			free(key);
			skip(ps);
			if (*ps->p == ',') {
				ps->p++;
				continue;
			}
			if (*ps->p == '}') {
				ps->p++;
				*out = map;
				return true;
			}
			fail(ps, "expected ',' or '}'");
			mp_value_release(map);
			return false;
		}
	}
	if (strncmp(ps->p, "true", 4) == 0) {
		ps->p += 4;
		*out = mp_bool(true);
		return true;
	}
	if (strncmp(ps->p, "false", 5) == 0) {
		ps->p += 5;
		*out = mp_bool(false);
		return true;
	}
	if (strncmp(ps->p, "null", 4) == 0) {
		ps->p += 4;
		*out = mp_none();
		return true;
	}
	fail(ps, c ? "unexpected character" : "unexpected end");
	return false;
}

static bool j_parse(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "parse(text) takes a string.");
	Parser ps;
	ps.p = argv[0].as.s;
	ps.err[0] = '\0';
	if (!parse_value(&ps, ret, 0))
		return mp_error(ret, ps.err);
	skip(&ps);
	if (*ps.p) {
		mp_value_release(*ret);
		return mp_error(ret, "invalid JSON: trailing characters");
	}
	return true;
}

static void dump_string(Buf *b, const char *s)
{
	add(b, "\"", 1);
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		char esc[8];
		switch (c) {
		case '"': adds(b, "\\\""); break;
		case '\\': adds(b, "\\\\"); break;
		case '\n': adds(b, "\\n"); break;
		case '\r': adds(b, "\\r"); break;
		case '\t': adds(b, "\\t"); break;
		default:
			if (c < 0x20) {
				snprintf(esc, sizeof esc, "\\u%04x", c);
				adds(b, esc);
			} else {
				add(b, (const char *)&c, 1);
			}
		}
	}
	add(b, "\"", 1);
}

static void newline(Buf *b, int indent, int level)
{
	if (!indent)
		return;
	add(b, "\n", 1);
	for (int i = 0; i < indent * level; i++)
		add(b, " ", 1);
}

static bool dump(Buf *b, Value v, int indent, int level, const char **err)
{
	char num[64];
	if (level > DEPTH_MAX) {
		*err = "value nested too deeply (or cyclic).";
		return false;
	}
	switch (v.type) {
	case VAL_NONE: adds(b, "null"); break;
	case VAL_BOOL: adds(b, v.as.b ? "true" : "false"); break;
	case VAL_INT:
		snprintf(num, sizeof num, "%lld", (long long)v.as.i);
		adds(b, num);
		break;
	case VAL_FLOAT:
		if (!isfinite(v.as.f)) {
			*err = "cannot encode nan or infinity.";
			return false;
		}
		for (int prec = 15; prec <= 17; prec++) {
			snprintf(num, sizeof num, "%.*g", prec, v.as.f);
			if (strtod(num, NULL) == v.as.f)
				break;
		}
		adds(b, num);
		if (!strpbrk(num, ".eE"))
			adds(b, ".0");
		break;
	case VAL_STRING: dump_string(b, v.as.s); break;
	case VAL_LIST:
		add(b, "[", 1);
		for (int i = 0; i < v.as.list->count; i++) {
			if (i)
				add(b, ",", 1);
			newline(b, indent, level + 1);
			if (!dump(b, v.as.list->items[i], indent, level + 1, err))
				return false;
		}
		if (v.as.list->count)
			newline(b, indent, level);
		add(b, "]", 1);
		break;
	case VAL_MAP:
		add(b, "{", 1);
		for (int i = 0; i < v.as.map->count; i++) {
			if (i)
				add(b, ",", 1);
			newline(b, indent, level + 1);
			dump_string(b, v.as.map->keys[i]);
			adds(b, indent ? ": " : ":");
			if (!dump(b, v.as.map->vals[i], indent, level + 1, err))
				return false;
		}
		if (v.as.map->count)
			newline(b, indent, level);
		add(b, "}", 1);
		break;
	default:
		*err = "cannot encode functions or modules.";
		return false;
	}
	return true;
}

static bool j_dump(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 2 && (argc == 1 || mp_is_int(argv[1])), "dump(value, indent) takes a value and an optional indent.");
	int indent = argc == 2 ? (int)mp_to_int(argv[1]) : 0;
	MP_CHECK(indent >= 0 && indent <= 16, "dump(value, indent) takes an indent from 0 to 16.");
	Buf b = { NULL, 0, 0 };
	const char *err = NULL;
	if (!dump(&b, argv[0], indent, 0, &err)) {
		free(b.s);
		return mp_error(ret, err);
	}
	*ret = mp_str(b.s ? b.s : "");
	free(b.s);
	return true;
}

MP_MODULE({"parse", j_parse}, {"dump", j_dump});
