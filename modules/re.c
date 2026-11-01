#include <regex.h>
#include "mplangpfunc.h"

#define GROUPS_MAX 10

static bool compile(regex_t *re, Value pat, Value *ret)
{
	int rc = regcomp(re, pat.as.s, REG_EXTENDED);
	if (rc != 0) {
		char msg[128];
		regerror(rc, re, msg, sizeof msg);
		return mp_errorf(ret, "bad pattern: %s", msg);
	}
	return true;
}

static Value group(const char *s, regmatch_t m)
{
	return m.rm_so < 0 ? mp_none() : mp_string(s + m.rm_so, (int)(m.rm_eo - m.rm_so));
}

static bool r_test(int argc, Value *argv, Value *ret)
{
	regex_t re;
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "test(pattern, text) takes two strings.");
	if (!compile(&re, argv[0], ret))
		return false;
	*ret = mp_bool(regexec(&re, argv[1].as.s, 0, NULL, 0) == 0);
	regfree(&re);
	return true;
}

static bool r_find(int argc, Value *argv, Value *ret)
{
	regex_t re;
	regmatch_t m[GROUPS_MAX];
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "find(pattern, text) takes two strings.");
	if (!compile(&re, argv[0], ret))
		return false;
	if (regexec(&re, argv[1].as.s, GROUPS_MAX, m, 0) != 0) {
		*ret = mp_none();
	} else {
		*ret = mp_list();
		for (size_t i = 0; i <= re.re_nsub && i < GROUPS_MAX; i++)
			mp_push(*ret, group(argv[1].as.s, m[i]));
	}
	regfree(&re);
	return true;
}

/* Calls fn(start, end, match[]) for each non-overlapping match. */
typedef void (*Each)(void *ctx, const char *s, regmatch_t *m, size_t ngroups);

static void each_match(regex_t *re, const char *s, Each fn, void *ctx)
{
	regmatch_t m[GROUPS_MAX];
	size_t off = 0, len = strlen(s);
	size_t ng = re->re_nsub + 1 < GROUPS_MAX ? re->re_nsub + 1 : GROUPS_MAX;
	while (off <= len) {
		int flags = off > 0 ? REG_NOTBOL : 0;
		if (regexec(re, s + off, GROUPS_MAX, m, flags) != 0)
			break;
		regmatch_t abs[GROUPS_MAX];
		for (size_t i = 0; i < GROUPS_MAX; i++) {
			abs[i].rm_so = m[i].rm_so < 0 ? -1 : m[i].rm_so + (regoff_t)off;
			abs[i].rm_eo = m[i].rm_eo < 0 ? -1 : m[i].rm_eo + (regoff_t)off;
		}
		fn(ctx, s, abs, ng);
		off = (size_t)abs[0].rm_eo;
		if (abs[0].rm_eo == abs[0].rm_so)
			off++;
	}
}

static void collect(void *ctx, const char *s, regmatch_t *m, size_t ng)
{
	(void)ng;
	mp_push(*(Value *)ctx, group(s, m[0]));
}

static bool r_findall(int argc, Value *argv, Value *ret)
{
	regex_t re;
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "findall(pattern, text) takes two strings.");
	if (!compile(&re, argv[0], ret))
		return false;
	*ret = mp_list();
	each_match(&re, argv[1].as.s, collect, ret);
	regfree(&re);
	return true;
}

typedef struct {
	const char *repl;
	char *out;
	size_t n, cap, last;
} Sub;

static void sub_add(Sub *u, const char *p, size_t n)
{
	if (u->n + n + 1 > u->cap) {
		size_t cap = (u->n + n + 1) * 2;
		char *o = mp_alloc(cap);
		if (u->n)
			memcpy(o, u->out, u->n);
		free(u->out);
		u->out = o;
		u->cap = cap;
	}
	memcpy(u->out + u->n, p, n);
	u->n += n;
}

static void substitute(void *ctx, const char *s, regmatch_t *m, size_t ng)
{
	Sub *u = ctx;
	sub_add(u, s + u->last, (size_t)m[0].rm_so - u->last);
	for (const char *r = u->repl; *r; r++) {
		if (*r == '\\' && r[1] >= '0' && r[1] <= '9') {
			size_t g = (size_t)(r[1] - '0');
			if (g < ng && m[g].rm_so >= 0)
				sub_add(u, s + m[g].rm_so, (size_t)(m[g].rm_eo - m[g].rm_so));
			r++;
		} else if (*r == '\\' && r[1] == '\\') {
			sub_add(u, "\\", 1);
			r++;
		} else {
			sub_add(u, r, 1);
		}
	}
	u->last = (size_t)m[0].rm_eo;
}

static bool r_sub(int argc, Value *argv, Value *ret)
{
	regex_t re;
	MP_CHECK(argc == 3 && mp_is_str(argv[0]) && mp_is_str(argv[1]) && mp_is_str(argv[2]),
	    "sub(pattern, replacement, text) takes three strings.");
	if (!compile(&re, argv[0], ret))
		return false;
	Sub u = { argv[1].as.s, NULL, 0, 0, 0 };
	const char *text = argv[2].as.s;
	each_match(&re, text, substitute, &u);
	sub_add(&u, text + u.last, strlen(text) - u.last);
	u.out[u.n] = '\0';
	*ret = mp_str(u.out);
	free(u.out);
	regfree(&re);
	return true;
}

typedef struct {
	Value list;
	size_t last;
} Split;

static void splitter(void *ctx, const char *s, regmatch_t *m, size_t ng)
{
	Split *sp = ctx;
	(void)ng;
	if (m[0].rm_eo == m[0].rm_so)
		return;
	mp_push(sp->list, mp_string(s + sp->last, (int)((size_t)m[0].rm_so - sp->last)));
	sp->last = (size_t)m[0].rm_eo;
}

static bool r_split(int argc, Value *argv, Value *ret)
{
	regex_t re;
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "split(pattern, text) takes two strings.");
	if (!compile(&re, argv[0], ret))
		return false;
	Split sp = { mp_list(), 0 };
	each_match(&re, argv[1].as.s, splitter, &sp);
	mp_push(sp.list, mp_str(argv[1].as.s + sp.last));
	*ret = sp.list;
	regfree(&re);
	return true;
}

MP_MODULE({"test", r_test}, {"find", r_find}, {"findall", r_findall}, {"sub", r_sub}, {"split", r_split});
