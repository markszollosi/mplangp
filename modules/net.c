#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "mplangpfunc.h"

#define RECV_MAX (1 << 20)

static bool ignore_pipe(void)
{
	static bool done;
	if (!done) {
		signal(SIGPIPE, SIG_IGN);
		done = true;
	}
	return true;
}

static bool is_port(Value v)
{
	return mp_is_int(v) && mp_to_int(v) >= 0 && mp_to_int(v) <= 65535;
}

static bool is_fd(Value v)
{
	return mp_is_int(v) && mp_to_int(v) >= 0 && mp_to_int(v) < (1 << 20);
}

static int connect_to(const char *host, int port, double timeout, const char **why)
{
	struct addrinfo hints, *res, *ai;
	char service[16];
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	snprintf(service, sizeof service, "%d", port);
	int rc = getaddrinfo(host, service, &hints, &res);
	if (rc != 0) {
		*why = gai_strerror(rc);
		return -1;
	}
	int fd = -1;
	*why = "connection failed";
	for (ai = res; ai && fd < 0; ai = ai->ai_next) {
		int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0)
			continue;
		int flags = fcntl(s, F_GETFL, 0);
		fcntl(s, F_SETFL, flags | O_NONBLOCK);
		int c = connect(s, ai->ai_addr, ai->ai_addrlen);
		if (c != 0 && errno == EINPROGRESS) {
			struct pollfd p = { s, POLLOUT, 0 };
			int pr = poll(&p, 1, (int)(timeout * 1000));
			int err = 0;
			socklen_t len = sizeof err;
			if (pr > 0 && getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
				c = 0;
			} else {
				*why = pr == 0 ? "timed out" : strerror(err ? err : errno);
			}
		} else if (c != 0) {
			*why = strerror(errno);
		}
		if (c == 0) {
			fcntl(s, F_SETFL, flags);
			fd = s;
		} else {
			close(s);
		}
	}
	freeaddrinfo(res);
	return fd;
}

static bool n_connect(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 2 && argc <= 3 && mp_is_str(argv[0]) && is_port(argv[1]), "connect(host, port, timeout) takes a string, a port and an optional number.");
	MP_CHECK(argc == 2 || mp_is_num(argv[2]), "connect(host, port, timeout) takes the timeout as a number.");
	ignore_pipe();
	const char *why;
	int fd = connect_to(argv[0].as.s, (int)mp_to_int(argv[1]), argc == 3 ? mp_to_float(argv[2]) : 10.0, &why);
	if (fd < 0)
		return mp_errorf(ret, "cannot connect to %s:%d: %s", argv[0].as.s, (int)mp_to_int(argv[1]), why);
	*ret = mp_int(fd);
	return true;
}

static bool n_open(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 2 && argc <= 3 && mp_is_str(argv[0]) && is_port(argv[1]), "open(host, port, timeout) takes a string, a port and an optional number.");
	MP_CHECK(argc == 2 || mp_is_num(argv[2]), "open(host, port, timeout) takes the timeout as a number.");
	const char *why;
	int fd = connect_to(argv[0].as.s, (int)mp_to_int(argv[1]), argc == 3 ? mp_to_float(argv[2]) : 2.0, &why);
	if (fd >= 0)
		close(fd);
	*ret = mp_bool(fd >= 0);
	return true;
}

static bool n_listen(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 2 && is_port(argv[0]), "listen(port, host) takes a port and an optional host string.");
	MP_CHECK(argc == 1 || mp_is_str(argv[1]), "listen(port, host) takes the host as a string.");
	const char *host = argc == 2 ? argv[1].as.s : "127.0.0.1";
	struct addrinfo hints, *res;
	char service[16];
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	snprintf(service, sizeof service, "%d", (int)mp_to_int(argv[0]));
	int rc = getaddrinfo(host, service, &hints, &res);
	if (rc != 0)
		return mp_errorf(ret, "cannot resolve '%s': %s", host, gai_strerror(rc));
	int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	int on = 1;
	if (fd >= 0) {
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
		if (bind(fd, res->ai_addr, res->ai_addrlen) != 0 || listen(fd, 16) != 0) {
			close(fd);
			fd = -1;
		}
	}
	freeaddrinfo(res);
	if (fd < 0)
		return mp_errorf(ret, "cannot listen on %s:%s: %s", host, service, strerror(errno));
	ignore_pipe();
	*ret = mp_int(fd);
	return true;
}

static bool n_port(int argc, Value *argv, Value *ret)
{
	struct sockaddr_storage ss;
	socklen_t len = sizeof ss;
	MP_CHECK(argc == 1 && is_fd(argv[0]), "port(fd) takes a socket.");
	MP_CHECK(getsockname((int)mp_to_int(argv[0]), (struct sockaddr *)&ss, &len) == 0, "port(fd): not a socket.");
	int port = ss.ss_family == AF_INET6 ? ntohs(((struct sockaddr_in6 *)&ss)->sin6_port) : ntohs(((struct sockaddr_in *)&ss)->sin_port);
	*ret = mp_int(port);
	return true;
}

static bool n_accept(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && is_fd(argv[0]), "accept(fd) takes a listening socket.");
	int fd;
	do {
		fd = accept((int)mp_to_int(argv[0]), NULL, NULL);
	} while (fd < 0 && errno == EINTR);
	MP_CHECK(fd >= 0, "accept failed.");
	*ret = mp_int(fd);
	return true;
}

static bool n_peer(int argc, Value *argv, Value *ret)
{
	struct sockaddr_storage ss;
	socklen_t len = sizeof ss;
	char buf[INET6_ADDRSTRLEN];
	MP_CHECK(argc == 1 && is_fd(argv[0]), "peer(fd) takes a socket.");
	MP_CHECK(getpeername((int)mp_to_int(argv[0]), (struct sockaddr *)&ss, &len) == 0, "peer(fd): not connected.");
	const void *addr = ss.ss_family == AF_INET6 ? (const void *)&((struct sockaddr_in6 *)&ss)->sin6_addr
	    : (const void *)&((struct sockaddr_in *)&ss)->sin_addr;
	MP_CHECK(inet_ntop(ss.ss_family, addr, buf, sizeof buf), "peer(fd) failed.");
	*ret = mp_str(buf);
	return true;
}

static bool n_send(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && is_fd(argv[0]) && mp_is_str(argv[1]), "send(fd, text) takes a socket and a string.");
	const char *p = argv[1].as.s;
	size_t left = strlen(p);
	int64_t total = 0;
	while (left > 0) {
		ssize_t n = write((int)mp_to_int(argv[0]), p, left);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			return mp_errorf(ret, "send failed: %s", strerror(errno));
		p += n;
		left -= (size_t)n;
		total += n;
	}
	*ret = mp_int(total);
	return true;
}

static bool n_recv(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && is_fd(argv[0]) && mp_is_int(argv[1]), "recv(fd, max) takes a socket and an integer.");
	int64_t max = mp_to_int(argv[1]);
	MP_CHECK(max >= 1 && max <= RECV_MAX, "recv(fd, max) takes a size from 1 to 1048576.");
	char *buf = mp_alloc((size_t)max);
	ssize_t n;
	do {
		n = read((int)mp_to_int(argv[0]), buf, (size_t)max);
	} while (n < 0 && errno == EINTR);
	if (n < 0) {
		free(buf);
		return mp_errorf(ret, "recv failed: %s", errno == EAGAIN ? "timed out" : strerror(errno));
	}
	*ret = mp_string_clean(buf, (size_t)n);
	free(buf);
	return true;
}

static bool n_readline(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && is_fd(argv[0]), "readline(fd) takes a socket.");
	size_t cap = 128, len = 0;
	char *buf = mp_alloc(cap);
	for (;;) {
		char c;
		ssize_t n = read((int)mp_to_int(argv[0]), &c, 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0) {
			free(buf);
			return mp_errorf(ret, "readline failed: %s", errno == EAGAIN ? "timed out" : strerror(errno));
		}
		if (n == 0) {
			if (len == 0) {
				free(buf);
				*ret = mp_none();
				return true;
			}
			break;
		}
		if (c == '\n')
			break;
		if (len + 1 >= cap) {
			char *bigger = mp_alloc(cap * 2);
			memcpy(bigger, buf, len);
			free(buf);
			buf = bigger;
			cap *= 2;
		}
		buf[len++] = c;
	}
	if (len && buf[len - 1] == '\r')
		len--;
	*ret = mp_string_clean(buf, len);
	free(buf);
	return true;
}

static bool n_timeout(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && is_fd(argv[0]) && mp_is_num(argv[1]), "timeout(fd, seconds) takes a socket and a number.");
	double s = mp_to_float(argv[1]);
	MP_CHECK(s >= 0 && s < 86400, "timeout(fd, seconds) takes a number from 0 to 86400.");
	struct timeval tv;
	tv.tv_sec = (time_t)s;
	tv.tv_usec = (suseconds_t)((s - (double)tv.tv_sec) * 1e6);
	int fd = (int)mp_to_int(argv[0]);
	MP_CHECK(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) == 0 && setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv) == 0, "timeout(fd): not a socket.");
	return true;
}

static bool n_close(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && is_fd(argv[0]), "close(fd) takes a socket.");
	close((int)mp_to_int(argv[0]));
	return true;
}

static bool n_resolve(int argc, Value *argv, Value *ret)
{
	struct addrinfo hints, *res, *ai;
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "resolve(host) takes a string.");
	memset(&hints, 0, sizeof hints);
	hints.ai_socktype = SOCK_STREAM;
	int rc = getaddrinfo(argv[0].as.s, NULL, &hints, &res);
	if (rc != 0)
		return mp_errorf(ret, "cannot resolve '%s': %s", argv[0].as.s, gai_strerror(rc));
	*ret = mp_list();
	for (ai = res; ai; ai = ai->ai_next) {
		char buf[INET6_ADDRSTRLEN];
		const void *addr = ai->ai_family == AF_INET6 ? (const void *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr
		    : (const void *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;
		if (!inet_ntop(ai->ai_family, addr, buf, sizeof buf))
			continue;
		bool seen = false;
		for (int i = 0; i < ret->as.list->count; i++)
			seen = seen || strcmp(ret->as.list->items[i].as.s, buf) == 0;
		if (!seen)
			mp_push(*ret, mp_str(buf));
	}
	freeaddrinfo(res);
	return true;
}

MP_MODULE({"connect", n_connect}, {"open", n_open}, {"listen", n_listen}, {"port", n_port},
	{"accept", n_accept}, {"peer", n_peer}, {"send", n_send}, {"recv", n_recv},
	{"readline", n_readline}, {"timeout", n_timeout}, {"close", n_close}, {"resolve", n_resolve});
