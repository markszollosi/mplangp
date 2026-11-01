#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mplangpfunc.h"

typedef struct {
	char *s;
	size_t n, cap;
} Out;

static void out_add(Out *o, const char *chars, size_t n)
{
	if (o->n + n + 1 > o->cap) {
		size_t cap = (o->n + n + 1) * 2;
		char *s = mp_alloc(cap);
		if (o->n)
			memcpy(s, o->s, o->n);
		free(o->s);
		o->s = s;
		o->cap = cap;
	}
	memcpy(o->s + o->n, chars, n);
	o->n += n;
}

static Value out_value(Out *o)
{
	Value v = mp_string_clean(o->s ? o->s : "", o->n);
	free(o->s);
	return v;
}

/* Runs argv, feeding input to stdin; fills result with code, out, err. */
static bool capture(char **args, const char *input, Value *ret)
{
	int in[2], out[2], err[2];
	if (pipe(in) != 0)
		return mp_error(ret, "cannot create a pipe.");
	if (pipe(out) != 0) {
		close(in[0]);
		close(in[1]);
		return mp_error(ret, "cannot create a pipe.");
	}
	if (pipe(err) != 0) {
		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		return mp_error(ret, "cannot create a pipe.");
	}
	signal(SIGPIPE, SIG_IGN);
	fflush(NULL);
	pid_t pid = fork();
	if (pid < 0) {
		close(in[0]); close(in[1]); close(out[0]); close(out[1]); close(err[0]); close(err[1]);
		return mp_error(ret, "fork failed.");
	}
	if (pid == 0) {
		dup2(in[0], 0);
		dup2(out[1], 1);
		dup2(err[1], 2);
		close(in[0]); close(in[1]); close(out[0]); close(out[1]); close(err[0]); close(err[1]);
		execvp(args[0], args);
		_exit(127);
	}
	close(in[0]);
	close(out[1]);
	close(err[1]);
	size_t inlen = input ? strlen(input) : 0, sent = 0;
	if (inlen == 0) {
		close(in[1]);
		in[1] = -1;
	} else {
		fcntl(in[1], F_SETFL, O_NONBLOCK);
	}
	Out o = { NULL, 0, 0 }, e = { NULL, 0, 0 };
	struct pollfd fds[3] = { { out[0], POLLIN, 0 }, { err[0], POLLIN, 0 }, { in[1], POLLOUT, 0 } };
	int open_fds = 2;
	char buf[4096];
	while (open_fds > 0 || in[1] >= 0) {
		fds[2].fd = in[1];
		int rc = poll(fds, 3, -1);
		if (rc < 0 && errno == EINTR)
			continue;
		if (rc < 0)
			break;
		for (int i = 0; i < 2; i++) {
			if (fds[i].fd < 0 || !(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
				continue;
			ssize_t n = read(fds[i].fd, buf, sizeof buf);
			if (n > 0) {
				out_add(i ? &e : &o, buf, (size_t)n);
			} else if (n == 0 || errno != EINTR) {
				close(fds[i].fd);
				fds[i].fd = -1;
				open_fds--;
			}
		}
		if (in[1] >= 0 && (fds[2].revents & (POLLOUT | POLLERR | POLLHUP))) {
			ssize_t n = write(in[1], input + sent, inlen - sent);
			if (n > 0)
				sent += (size_t)n;
			if (n < 0 && errno == EAGAIN)
				continue;
			if (n < 0 || sent >= inlen) {
				close(in[1]);
				in[1] = -1;
			}
		}
	}
	if (in[1] >= 0)
		close(in[1]);
	int status = 0;
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
	int code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? -WTERMSIG(status) : -1;
	*ret = mp_map();
	mp_set(*ret, "code", mp_int(code));
	mp_set(*ret, "out", out_value(&o));
	mp_set(*ret, "err", out_value(&e));
	return true;
}

static bool p_run(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 2, "run(argv, input) takes a list of strings and optional input.");
	MP_CHECK(argv[0].type == VAL_LIST && argv[0].as.list->count > 0, "run(argv) needs a non-empty list of strings.");
	MP_CHECK(argc == 1 || mp_is_str(argv[1]), "run(argv, input) takes input as a string.");
	List *l = argv[0].as.list;
	char **args = mp_alloc(sizeof *args * (size_t)(l->count + 1));
	for (int i = 0; i < l->count; i++) {
		if (!mp_is_str(l->items[i])) {
			free(args);
			return mp_error(ret, "run(argv) needs a list of strings.");
		}
		args[i] = l->items[i].as.s;
	}
	args[l->count] = NULL;
	bool ok = capture(args, argc == 2 ? argv[1].as.s : NULL, ret);
	free(args);
	return ok;
}

static bool p_sh(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 2 && mp_is_str(argv[0]), "sh(command, input) takes a string and optional input.");
	MP_CHECK(argc == 1 || mp_is_str(argv[1]), "sh(command, input) takes input as a string.");
	char *args[] = { "/bin/sh", "-c", argv[0].as.s, NULL };
	return capture(args, argc == 2 ? argv[1].as.s : NULL, ret);
}

static bool p_which(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "which(name) takes a string.");
	const char *name = argv[0].as.s;
	*ret = mp_none();
	if (strchr(name, '/')) {
		if (access(name, X_OK) == 0)
			*ret = mp_str(name);
		return true;
	}
	const char *path = getenv("PATH");
	while (path && *path) {
		const char *colon = strchr(path, ':');
		size_t n = colon ? (size_t)(colon - path) : strlen(path);
		char *full = mp_alloc(n + strlen(name) + 2);
		memcpy(full, path, n);
		full[n] = '/';
		strcpy(full + n + 1, name);
		struct stat st;
		bool found = stat(full, &st) == 0 && S_ISREG(st.st_mode) && access(full, X_OK) == 0;
		if (found)
			*ret = mp_str(full);
		free(full);
		if (found)
			break;
		path = colon ? colon + 1 : NULL;
	}
	return true;
}

MP_MODULE({"run", p_run}, {"sh", p_sh}, {"which", p_which});
