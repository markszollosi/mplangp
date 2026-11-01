#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mplangpfunc.h"

static int by_name(const void *a, const void *b)
{
	return strcmp(((const Value *)a)->as.s, ((const Value *)b)->as.s);
}

static bool fail(Value *ret, const char *what, const char *path)
{
	return mp_errorf(ret, "%s '%s': %s", what, path, strerror(errno));
}

static bool f_read(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "read(path) takes a string.");
	FILE *f = fopen(argv[0].as.s, "rb");
	if (!f)
		return fail(ret, "cannot read", argv[0].as.s);
	size_t cap = 4096, len = 0;
	char *buf = mp_alloc(cap);
	for (;;) {
		size_t n = fread(buf + len, 1, cap - len, f);
		len += n;
		if (len < cap)
			break;
		char *bigger = mp_alloc(cap * 2);
		memcpy(bigger, buf, len);
		free(buf);
		buf = bigger;
		cap *= 2;
	}
	bool bad = ferror(f);
	fclose(f);
	if (bad) {
		free(buf);
		return fail(ret, "cannot read", argv[0].as.s);
	}
	*ret = mp_string_clean(buf, len);
	free(buf);
	return true;
}

static bool put(Value *argv, Value *ret, const char *mode, const char *name)
{
	FILE *f = fopen(argv[0].as.s, mode);
	if (!f)
		return fail(ret, name, argv[0].as.s);
	size_t n = strlen(argv[1].as.s);
	bool ok = fwrite(argv[1].as.s, 1, n, f) == n;
	ok = fclose(f) == 0 && ok;
	return ok ? true : fail(ret, name, argv[0].as.s);
}

static bool f_write(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "write(path, text) takes two strings.");
	return put(argv, ret, "wb", "cannot write");
}

static bool f_append(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "append(path, text) takes two strings.");
	return put(argv, ret, "ab", "cannot append to");
}

static bool f_exists(int argc, Value *argv, Value *ret)
{
	struct stat st;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "exists(path) takes a string.");
	*ret = mp_bool(stat(argv[0].as.s, &st) == 0);
	return true;
}

static bool f_isdir(int argc, Value *argv, Value *ret)
{
	struct stat st;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "isdir(path) takes a string.");
	*ret = mp_bool(stat(argv[0].as.s, &st) == 0 && S_ISDIR(st.st_mode));
	return true;
}

static bool f_isfile(int argc, Value *argv, Value *ret)
{
	struct stat st;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "isfile(path) takes a string.");
	*ret = mp_bool(stat(argv[0].as.s, &st) == 0 && S_ISREG(st.st_mode));
	return true;
}

static bool f_stat(int argc, Value *argv, Value *ret)
{
	struct stat st;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "stat(path) takes a string.");
	if (stat(argv[0].as.s, &st) != 0)
		return fail(ret, "cannot stat", argv[0].as.s);
	*ret = mp_map();
	mp_set(*ret, "size", mp_int(st.st_size));
	mp_set(*ret, "mtime", mp_int(st.st_mtime));
	mp_set(*ret, "mode", mp_int(st.st_mode & 07777));
	mp_set(*ret, "dir", mp_bool(S_ISDIR(st.st_mode)));
	return true;
}

static bool f_list(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "list(dir) takes a string.");
	DIR *d = opendir(argv[0].as.s);
	if (!d)
		return fail(ret, "cannot list", argv[0].as.s);
	*ret = mp_list();
	struct dirent *e;
	while ((e = readdir(d)))
		if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
			mp_push(*ret, mp_str(e->d_name));
	closedir(d);
	if (ret->as.list->count > 1)
		qsort(ret->as.list->items, (size_t)ret->as.list->count, sizeof(Value), by_name);
	return true;
}

static bool f_glob(int argc, Value *argv, Value *ret)
{
	glob_t g;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "glob(pattern) takes a string.");
	int rc = glob(argv[0].as.s, 0, NULL, &g);
	if (rc != 0 && rc != GLOB_NOMATCH)
		return mp_error(ret, "glob failed.");
	*ret = mp_list();
	if (rc == 0)
		for (size_t i = 0; i < g.gl_pathc; i++)
			mp_push(*ret, mp_str(g.gl_pathv[i]));
	globfree(&g);
	return true;
}

static bool mkdirs(char *path)
{
	for (char *p = path + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(path, 0777) != 0 && errno != EEXIST) {
			*p = '/';
			return false;
		}
		*p = '/';
	}
	return mkdir(path, 0777) == 0 || errno == EEXIST;
}

static bool f_mkdir(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "mkdir(path) takes a string.");
	char *copy = mp_alloc(strlen(argv[0].as.s) + 1);
	strcpy(copy, argv[0].as.s);
	bool ok = mkdirs(copy);
	free(copy);
	return ok ? true : fail(ret, "cannot create", argv[0].as.s);
}

static bool f_remove(int argc, Value *argv, Value *ret)
{
	struct stat st;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "remove(path) takes a string.");
	if (lstat(argv[0].as.s, &st) != 0)
		return fail(ret, "cannot remove", argv[0].as.s);
	int rc = S_ISDIR(st.st_mode) ? rmdir(argv[0].as.s) : unlink(argv[0].as.s);
	return rc == 0 ? true : fail(ret, "cannot remove", argv[0].as.s);
}

static bool f_rename(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "rename(from, to) takes two strings.");
	return rename(argv[0].as.s, argv[1].as.s) == 0 ? true : fail(ret, "cannot rename", argv[0].as.s);
}

static bool f_copy(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "copy(from, to) takes two strings.");
	FILE *in = fopen(argv[0].as.s, "rb");
	if (!in)
		return fail(ret, "cannot read", argv[0].as.s);
	FILE *out = fopen(argv[1].as.s, "wb");
	if (!out) {
		fclose(in);
		return fail(ret, "cannot write", argv[1].as.s);
	}
	char buf[8192];
	size_t n;
	bool ok = true;
	while (ok && (n = fread(buf, 1, sizeof buf, in)) > 0)
		ok = fwrite(buf, 1, n, out) == n;
	ok = fclose(out) == 0 && ok;
	fclose(in);
	return ok ? true : fail(ret, "cannot copy to", argv[1].as.s);
}

static bool f_cwd(int argc, Value *argv, Value *ret)
{
	char buf[4096];
	(void)argv;
	MP_CHECK(argc == 0, "cwd() takes no arguments.");
	if (!getcwd(buf, sizeof buf))
		return mp_error(ret, "cannot get the working directory.");
	*ret = mp_str(buf);
	return true;
}

static bool f_chdir(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "chdir(path) takes a string.");
	return chdir(argv[0].as.s) == 0 ? true : fail(ret, "cannot enter", argv[0].as.s);
}

MP_MODULE({"read", f_read}, {"write", f_write}, {"append", f_append}, {"exists", f_exists},
	{"isdir", f_isdir}, {"isfile", f_isfile}, {"stat", f_stat}, {"list", f_list},
	{"glob", f_glob}, {"mkdir", f_mkdir}, {"remove", f_remove}, {"rename", f_rename},
	{"copy", f_copy}, {"cwd", f_cwd}, {"chdir", f_chdir});
