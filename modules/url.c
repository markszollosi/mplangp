#include <ctype.h>
#include "mplangpfunc.h"

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static char *decode(const char *s, size_t n)
{
	char *out = mp_alloc(n + 1);
	size_t o = 0;
	for (size_t i = 0; i < n; i++) {
		if (s[i] == '%' && i + 2 < n &&hexval(s[i + 1]) >= 0 && hexval(s[i + 2]) >= 0) {
			int v = hexval(s[i + 1]) * 16 + hexval(s[i + 2]);
			if (v)
				out[o++] = (char)v;
			i += 2;
		} else {
			out[o++] = s[i] == '+' ? ' ' : s[i];
		}
	}
	out[o] = '\0';
	return out;
}

static size_t encode(char *out, const char *s)
{
	size_t o = 0;
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			out[o++] = (char)c;
		else
			o += (size_t)sprintf(out + o, "%%%02X", c);
	}
	out[o] = '\0';
	return o;
}

static Value piece(const char *s, size_t n)
{
	return mp_string(s, (int)n);
}

static bool u_parse(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "parse(url) takes a string.");
	const char *u = argv[0].as.s;
	const char *p = u;
	Value scheme = mp_str(""), user = mp_str(""), host = mp_str(""), port = mp_none();
	const char *colon = strstr(p, "://");
	if (colon) {
		for (const char *c = p; c < colon; c++)
			MP_CHECK(isalnum((unsigned char)*c) || *c == '+' || *c == '-' || *c == '.', "parse(url): bad scheme.");
		mp_value_release(scheme);
		scheme = piece(p, (size_t)(colon - p));
		p = colon + 3;
		size_t alen = strcspn(p, "/?#");
		const char *auth = p, *aend = p + alen;
		const char *at = memchr(auth, '@', alen);
		if (at) {
			mp_value_release(user);
			user = piece(auth, (size_t)(at - auth));
			auth = at + 1;
		}
		const char *pc = NULL;
		if (auth < aend && *auth == '[') {
			const char *rb = memchr(auth, ']', (size_t)(aend - auth));
			pc = rb && rb + 1 < aend && rb[1] == ':' ? rb + 1 : NULL;
		} else {
			for (const char *c = auth; c < aend; c++)
				if (*c == ':')
					pc = c;
		}
		mp_value_release(host);
		host = piece(auth, (size_t)((pc ? pc : aend) - auth));
		if (pc && pc + 1 < aend) {
			char num[16];
			size_t nl = (size_t)(aend - pc - 1);
			bool ok = nl < sizeof num;
			for (size_t i = 0; ok && i < nl; i++)
				ok = isdigit((unsigned char)pc[1 + i]) != 0;
			if (!ok) {
				mp_value_release(scheme);
				mp_value_release(user);
				mp_value_release(host);
				return mp_error(ret, "parse(url): bad port.");
			}
			memcpy(num, pc + 1, nl);
			num[nl] = '\0';
			port = mp_int(atoi(num));
		}
		p = aend;
	}
	size_t plen = strcspn(p, "?#");
	Value path = piece(p, plen);
	p += plen;
	Value query = mp_str(""), frag = mp_str("");
	if (*p == '?') {
		size_t ql = strcspn(p + 1, "#");
		mp_value_release(query);
		query = piece(p + 1, ql);
		p += 1 + ql;
	}
	if (*p == '#') {
		mp_value_release(frag);
		frag = mp_str(p + 1);
	}
	Value params = mp_map();
	const char *q = query.as.s;
	while (*q) {
		size_t pl = strcspn(q, "&");
		if (pl) {
			const char *eq = memchr(q, '=', pl);
			size_t kl = eq ? (size_t)(eq - q) : pl;
			char *k = decode(q, kl);
			char *v = eq ? decode(eq + 1, pl - kl - 1) : decode("", 0);
			mp_set(params, k, mp_str(v));
			free(k);
			free(v);
		}
		q += pl + (q[pl] == '&');
	}
	*ret = mp_map();
	mp_set(*ret, "scheme", scheme);
	mp_set(*ret, "user", user);
	mp_set(*ret, "host", host);
	mp_set(*ret, "port", port);
	mp_set(*ret, "path", path);
	mp_set(*ret, "query", query);
	mp_set(*ret, "fragment", frag);
	mp_set(*ret, "params", params);
	return true;
}

static bool u_query(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && argv[0].type == VAL_MAP, "query(map) takes a map.");
	Map *m = argv[0].as.map;
	size_t cap = 1;
	for (int i = 0; i < m->count; i++)
		cap += strlen(m->keys[i]) * 3 + 2 + 64 + (mp_is_str(m->vals[i]) ? strlen(m->vals[i].as.s) * 3 : 0);
	char *out = mp_alloc(cap);
	size_t o = 0;
	for (int i = 0; i < m->count; i++) {
		Value v = m->vals[i];
		char num[64];
		const char *text;
		if (mp_is_str(v)) {
			text = v.as.s;
		} else if (v.type == VAL_INT) {
			snprintf(num, sizeof num, "%lld", (long long)v.as.i);
			text = num;
		} else if (v.type == VAL_FLOAT) {
			snprintf(num, sizeof num, "%.12g", v.as.f);
			text = num;
		} else if (v.type == VAL_BOOL) {
			text = v.as.b ? "true" : "false";
		} else if (v.type == VAL_NONE) {
			text = "";
		} else {
			free(out);
			return mp_error(ret, "query(map): values must be strings, numbers, bools or none.");
		}
		if (i)
			out[o++] = '&';
		o += encode(out + o, m->keys[i]);
		out[o++] = '=';
		o += encode(out + o, text);
	}
	out[o] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

MP_MODULE({"parse", u_parse}, {"query", u_query});
