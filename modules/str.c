#include <ctype.h>
#include "mplangpfunc.h"

static bool s_len(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_STRING)
		return mp_error(ret, "len(s) takes one string.");
	*ret = mp_int((int64_t)strlen(argv[0].as.s));
	return true;
}

static bool s_upper(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_STRING)
		return mp_error(ret, "upper(s) takes one string.");
	*ret = mp_string(argv[0].as.s, (int)strlen(argv[0].as.s));
	for (char *c = ret->as.s; *c; c++)
		*c = (char)toupper((unsigned char)*c);
	return true;
}

static bool s_lower(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "lower(s) takes one string.");
	*ret = mp_str(argv[0].as.s);
	for (char *c = ret->as.s; *c; c++)
		*c = (char)tolower((unsigned char)*c);
	return true;
}

static bool s_trim(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "trim(s) takes one string.");
	const char *s = argv[0].as.s;
	size_t n = strlen(s);
	while (n > 0 && isspace((unsigned char)s[n - 1]))
		n--;
	while (n > 0 && isspace((unsigned char)*s)) {
		s++;
		n--;
	}
	*ret = mp_string(s, (int)n);
	return true;
}

static bool s_split(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "split(s, sep) takes two strings.");
	const char *s = argv[0].as.s, *sep = argv[1].as.s;
	size_t sl = strlen(sep);
	*ret = mp_list();
	if (sl == 0) {
		for (; *s; s++)
			mp_push(*ret, mp_string(s, 1));
		return true;
	}
	for (;;) {
		const char *hit = strstr(s, sep);
		if (!hit) {
			mp_push(*ret, mp_str(s));
			return true;
		}
		mp_push(*ret, mp_string(s, (int)(hit - s)));
		s = hit + sl;
	}
}

static bool s_lines(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "lines(s) takes one string.");
	const char *s = argv[0].as.s;
	*ret = mp_list();
	while (*s) {
		size_t n = strcspn(s, "\n");
		size_t e = n;
		if (e && s[e - 1] == '\r')
			e--;
		mp_push(*ret, mp_string(s, (int)e));
		s += n + (s[n] == '\n');
	}
	return true;
}

static bool s_join(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && argv[0].type == VAL_LIST && mp_is_str(argv[1]), "join(list, sep) takes a list and a string.");
	List *l = argv[0].as.list;
	const char *sep = argv[1].as.s;
	size_t cap = 1, n = 0;
	for (int i = 0; i < l->count; i++)
		cap += (mp_is_str(l->items[i]) ? strlen(l->items[i].as.s) : 64) + strlen(sep);
	char *out = mp_alloc(cap);
	for (int i = 0; i < l->count; i++) {
		Value v = l->items[i];
		if (i) {
			strcpy(out + n, sep);
			n += strlen(sep);
		}
		if (mp_is_str(v)) {
			strcpy(out + n, v.as.s);
		} else if (v.type == VAL_INT) {
			snprintf(out + n, 64, "%lld", (long long)v.as.i);
		} else if (v.type == VAL_FLOAT) {
			snprintf(out + n, 64, "%.12g", v.as.f);
		} else if (v.type == VAL_BOOL) {
			strcpy(out + n, v.as.b ? "True" : "False");
		} else if (v.type == VAL_NONE) {
			strcpy(out + n, "None");
		} else {
			free(out);
			return mp_error(ret, "join(list, sep): items must be strings, numbers, bools or none.");
		}
		n += strlen(out + n);
	}
	if (l->count == 0)
		out[0] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool s_replace(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 3 && mp_is_str(argv[0]) && mp_is_str(argv[1]) && mp_is_str(argv[2]), "replace(s, old, new) takes three strings.");
	const char *s = argv[0].as.s, *old = argv[1].as.s, *rep = argv[2].as.s;
	size_t ol = strlen(old), rl = strlen(rep);
	MP_CHECK(ol > 0, "replace(s, old, new): old must not be empty.");
	size_t count = 0;
	for (const char *p = s; (p = strstr(p, old)); p += ol)
		count++;
	char *out = mp_alloc(strlen(s) + count * (rl > ol ? rl - ol : 0) + 1);
	char *o = out;
	for (;;) {
		const char *hit = strstr(s, old);
		if (!hit) {
			strcpy(o, s);
			break;
		}
		memcpy(o, s, (size_t)(hit - s));
		o += hit - s;
		memcpy(o, rep, rl);
		o += rl;
		s = hit + ol;
	}
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool s_find(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "find(s, needle) takes two strings.");
	const char *hit = strstr(argv[0].as.s, argv[1].as.s);
	*ret = mp_int(hit ? hit - argv[0].as.s : -1);
	return true;
}

static bool s_starts(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "starts(s, prefix) takes two strings.");
	*ret = mp_bool(strncmp(argv[0].as.s, argv[1].as.s, strlen(argv[1].as.s)) == 0);
	return true;
}

static bool s_ends(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "ends(s, suffix) takes two strings.");
	size_t n = strlen(argv[0].as.s), m = strlen(argv[1].as.s);
	*ret = mp_bool(m <= n && strcmp(argv[0].as.s + n - m, argv[1].as.s) == 0);
	return true;
}

MP_MODULE({"len", s_len}, {"upper", s_upper}, {"lower", s_lower}, {"trim", s_trim},
	{"split", s_split}, {"lines", s_lines}, {"join", s_join}, {"replace", s_replace},
	{"find", s_find}, {"starts", s_starts}, {"ends", s_ends});
