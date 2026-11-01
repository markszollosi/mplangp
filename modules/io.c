#include <sys/ioctl.h>
#include <unistd.h>
#include "mplangpfunc.h"

static bool i_input(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc <= 1 && (argc == 0 || mp_is_str(argv[0])), "input(prompt) takes an optional string.");
	if (argc == 1) {
		fputs(argv[0].as.s, stdout);
		fflush(stdout);
	}
	size_t cap = 128, len = 0;
	char *buf = mp_alloc(cap);
	int c;
	while ((c = getchar()) != EOF && c != '\n') {
		if (len + 1 >= cap) {
			char *bigger = mp_alloc(cap * 2);
			memcpy(bigger, buf, len);
			free(buf);
			buf = bigger;
			cap *= 2;
		}
		buf[len++] = (char)c;
	}
	if (c == EOF && len == 0) {
		free(buf);
		*ret = mp_none();
		return true;
	}
	if (len && buf[len - 1] == '\r')
		len--;
	*ret = mp_string_clean(buf, len);
	free(buf);
	return true;
}

static bool i_readall(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "readall() takes no arguments.");
	size_t cap = 4096, len = 0;
	char *buf = mp_alloc(cap);
	size_t n;
	while ((n = fread(buf + len, 1, cap - len, stdin)) > 0) {
		len += n;
		if (len == cap) {
			char *bigger = mp_alloc(cap * 2);
			memcpy(bigger, buf, len);
			free(buf);
			buf = bigger;
			cap *= 2;
		}
	}
	*ret = mp_string_clean(buf, len);
	free(buf);
	return true;
}

static bool i_write(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "write(text) takes a string.");
	fputs(argv[0].as.s, stdout);
	return true;
}

static bool i_eprint(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "eprint(text) takes a string.");
	fflush(stdout);
	fprintf(stderr, "%s\n", argv[0].as.s);
	return true;
}

static bool i_flush(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "flush() takes no arguments.");
	fflush(stdout);
	return true;
}

static bool i_isatty(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc <= 1 && (argc == 0 || mp_is_int(argv[0])), "isatty(fd) takes an optional integer.");
	*ret = mp_bool(isatty(argc ? (int)mp_to_int(argv[0]) : 1));
	return true;
}

static bool i_size(int argc, Value *argv, Value *ret)
{
	struct winsize ws;
	(void)argv;
	MP_CHECK(argc == 0, "size() takes no arguments.");
	int cols = 80, rows = 24;
	if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
		cols = ws.ws_col;
		rows = ws.ws_row;
	}
	*ret = mp_map();
	mp_set(*ret, "cols", mp_int(cols));
	mp_set(*ret, "rows", mp_int(rows));
	return true;
}

static const struct {
	const char *name;
	const char *code;
} styles[] = {
	{"reset", "0"}, {"bold", "1"}, {"dim", "2"}, {"underline", "4"},
	{"black", "30"}, {"red", "31"}, {"green", "32"}, {"yellow", "33"},
	{"blue", "34"}, {"magenta", "35"}, {"cyan", "36"}, {"white", "37"}, {"gray", "90"}
};

static bool i_color(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "color(name, text) takes two strings.");
	for (size_t i = 0; i < sizeof styles / sizeof styles[0]; i++) {
		if (strcmp(styles[i].name, argv[0].as.s) == 0) {
			char *out = mp_alloc(strlen(argv[1].as.s) + 16);
			sprintf(out, "\033[%sm%s\033[0m", styles[i].code, argv[1].as.s);
			*ret = mp_str(out);
			free(out);
			return true;
		}
	}
	return mp_errorf(ret, "color: unknown style '%s'.", argv[0].as.s);
}

MP_MODULE({"input", i_input}, {"readall", i_readall}, {"write", i_write}, {"eprint", i_eprint},
	{"flush", i_flush}, {"isatty", i_isatty}, {"size", i_size}, {"color", i_color});
