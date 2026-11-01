#include <unistd.h>
#include "mplangpfunc.h"

static bool p_join(int argc, Value *argv, Value *ret)
{
	size_t total = 2;
	MP_CHECK(argc >= 1, "join(a, b, ...) takes at least one string.");
	for (int i = 0; i < argc; i++) {
		MP_CHECK(mp_is_str(argv[i]), "join(a, b, ...) takes strings.");
		total += strlen(argv[i].as.s) + 1;
	}
	char *out = mp_alloc(total);
	out[0] = '\0';
	for (int i = 0; i < argc; i++) {
		const char *part = argv[i].as.s;
		if (!*part)
			continue;
		if (part[0] == '/')
			out[0] = '\0';
		size_t n = strlen(out);
		if (n && out[n - 1] != '/')
			out[n++] = '/';
		strcpy(out + n, part);
	}
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool p_base(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "base(path) takes a string.");
	const char *s = argv[0].as.s;
	size_t n = strlen(s);
	while (n > 1 && s[n - 1] == '/')
		n--;
	size_t start = n;
	while (start > 0 && s[start - 1] != '/')
		start--;
	*ret = mp_string(s + start, (int)(n - start));
	return true;
}

static bool p_dir(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "dir(path) takes a string.");
	const char *s = argv[0].as.s;
	size_t n = strlen(s);
	while (n > 1 && s[n - 1] == '/')
		n--;
	while (n > 0 && s[n - 1] != '/')
		n--;
	while (n > 1 && s[n - 1] == '/')
		n--;
	*ret = n == 0 ? mp_str(".") : mp_string(s, (int)n);
	return true;
}

static bool p_ext(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "ext(path) takes a string.");
	const char *s = argv[0].as.s;
	const char *slash = strrchr(s, '/');
	const char *base = slash ? slash + 1 : s;
	const char *dot = strrchr(base, '.');
	*ret = dot && dot != base ? mp_str(dot) : mp_str("");
	return true;
}

static bool p_stem(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "stem(path) takes a string.");
	const char *s = argv[0].as.s;
	const char *slash = strrchr(s, '/');
	const char *base = slash ? slash + 1 : s;
	const char *dot = strrchr(base, '.');
	*ret = mp_string(base, (int)(dot && dot != base ? dot - base : (long)strlen(base)));
	return true;
}

static char *normalize(const char *path)
{
	size_t n = strlen(path);
	char *out = mp_alloc(n + 2);
	const char **seg = mp_alloc(sizeof *seg * (n / 2 + 2));
	size_t *len = mp_alloc(sizeof *len * (n / 2 + 2));
	int count = 0;
	bool abs = path[0] == '/';
	const char *p = path;
	while (*p) {
		while (*p == '/')
			p++;
		const char *start = p;
		while (*p && *p != '/')
			p++;
		size_t l = (size_t)(p - start);
		if (l == 0 || (l == 1 && start[0] == '.'))
			continue;
		if (l == 2 && start[0] == '.' && start[1] == '.') {
			if (count > 0 && !(len[count - 1] == 2 && seg[count - 1][0] == '.' && seg[count - 1][1] == '.')) {
				count--;
				continue;
			}
			if (abs)
				continue;
		}
		seg[count] = start;
		len[count++] = l;
	}
	size_t o = 0;
	if (abs)
		out[o++] = '/';
	for (int i = 0; i < count; i++) {
		if (i)
			out[o++] = '/';
		memcpy(out + o, seg[i], len[i]);
		o += len[i];
	}
	if (o == 0)
		out[o++] = '.';
	out[o] = '\0';
	free(seg);
	free(len);
	return out;
}

static bool p_norm(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "norm(path) takes a string.");
	char *s = normalize(argv[0].as.s);
	*ret = mp_str(s);
	free(s);
	return true;
}

static bool p_abs(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "abs(path) takes a string.");
	const char *p = argv[0].as.s;
	char cwd[4096];
	char *full;
	if (p[0] == '/') {
		full = mp_alloc(strlen(p) + 1);
		strcpy(full, p);
	} else {
		if (!getcwd(cwd, sizeof cwd))
			return mp_error(ret, "cannot get the working directory.");
		full = mp_alloc(strlen(cwd) + strlen(p) + 2);
		sprintf(full, "%s/%s", cwd, p);
	}
	char *s = normalize(full);
	*ret = mp_str(s);
	free(s);
	free(full);
	return true;
}

static bool p_home(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "home() takes no arguments.");
	const char *h = getenv("HOME");
	*ret = h ? mp_str(h) : mp_none();
	return true;
}

MP_MODULE({"join", p_join}, {"base", p_base}, {"dir", p_dir}, {"ext", p_ext}, {"stem", p_stem},
	{"norm", p_norm}, {"abs", p_abs}, {"home", p_home});
