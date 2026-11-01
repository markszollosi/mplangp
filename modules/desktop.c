#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mplangpfunc.h"

/* Runs argv with input on stdin; stdout is captured when out is not NULL. Returns the exit code or -1. */
static int spawn(char **args, const char *input, char **out)
{
	int in[2], op[2];
	if (pipe(in) != 0)
		return -1;
	if (pipe(op) != 0) {
		close(in[0]);
		close(in[1]);
		return -1;
	}
	signal(SIGPIPE, SIG_IGN);
	fflush(NULL);
	pid_t pid = fork();
	if (pid < 0) {
		close(in[0]); close(in[1]); close(op[0]); close(op[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(in[0], 0);
		if (out)
			dup2(op[1], 1);
		close(in[0]); close(in[1]); close(op[0]); close(op[1]);
		execvp(args[0], args);
		_exit(127);
	}
	close(in[0]);
	close(op[1]);
	if (input) {
		size_t left = strlen(input);
		while (left > 0) {
			ssize_t n = write(in[1], input, left);
			if (n <= 0)
				break;
			input += n;
			left -= (size_t)n;
		}
	}
	close(in[1]);
	size_t cap = 256, len = 0;
	char *buf = mp_alloc(cap);
	ssize_t n;
	while ((n = read(op[0], buf + len, cap - len - 1)) > 0 || (n < 0 && errno == EINTR)) {
		if (n < 0)
			continue;
		len += (size_t)n;
		if (len + 1 >= cap) {
			char *bigger = mp_alloc(cap * 2);
			memcpy(bigger, buf, len);
			free(buf);
			buf = bigger;
			cap *= 2;
		}
	}
	close(op[0]);
	buf[len] = '\0';
	int status = 0;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	if (out)
		*out = buf;
	else
		free(buf);
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

#ifdef __APPLE__
#define CLIP_GET { "pbpaste", NULL }
#define CLIP_SET { "pbcopy", NULL }
#define OPENER "open"
#else
#define CLIP_GET { "xclip", "-o", "-selection", "clipboard", NULL }
#define CLIP_SET { "xclip", "-i", "-selection", "clipboard", NULL }
#define OPENER "xdg-open"
#endif

static bool d_clipboard(int argc, Value *argv, Value *ret)
{
	char *args[] = CLIP_GET;
	char *out = NULL;
	(void)argv;
	MP_CHECK(argc == 0, "clipboard() takes no arguments.");
	int rc = spawn(args, NULL, &out);
	if (rc != 0) {
		free(out);
		return mp_error(ret, "clipboard is unavailable.");
	}
	*ret = mp_string_clean(out, strlen(out));
	free(out);
	return true;
}

static bool d_copy(int argc, Value *argv, Value *ret)
{
	char *args[] = CLIP_SET;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "copy(text) takes a string.");
	MP_CHECK(spawn(args, argv[0].as.s, NULL) == 0, "clipboard is unavailable.");
	return true;
}

static bool d_notify(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "notify(title, text) takes two strings.");
#ifdef __APPLE__
	char *args[] = { "osascript", "-e", "on run {t, b}", "-e", "display notification b with title t",
	    "-e", "end run", argv[0].as.s, argv[1].as.s, NULL };
#else
	char *args[] = { "notify-send", "--", argv[0].as.s, argv[1].as.s, NULL };
#endif
	MP_CHECK(spawn(args, NULL, NULL) == 0, "notifications are unavailable.");
	return true;
}

static bool d_open(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "open(target) takes a path or url string.");
	MP_CHECK(argv[0].as.s[0] && argv[0].as.s[0] != '-', "open(target): the target must not be empty or start with '-'.");
	char *args[] = { OPENER, argv[0].as.s, NULL };
	MP_CHECK(spawn(args, NULL, NULL) == 0, "cannot open the target.");
	return true;
}

MP_MODULE({"clipboard", d_clipboard}, {"copy", d_copy}, {"notify", d_notify}, {"open", d_open});
