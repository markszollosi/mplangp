#include <ctype.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "mplangpfunc.h"

extern char **environ;

static bool o_env(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "env(name) takes a string.");
	const char *v = getenv(argv[0].as.s);
	*ret = v ? mp_str(v) : mp_none();
	return true;
}

static bool o_setenv(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "setenv(name, value) takes two strings.");
	MP_CHECK(setenv(argv[0].as.s, argv[1].as.s, 1) == 0, "setenv failed.");
	return true;
}

static bool o_unsetenv(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "unsetenv(name) takes a string.");
	unsetenv(argv[0].as.s);
	return true;
}

static bool o_environ(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "environ() takes no arguments.");
	*ret = mp_map();
	for (char **e = environ; e && *e; e++) {
		const char *eq = strchr(*e, '=');
		if (!eq)
			continue;
		char *name = mp_alloc((size_t)(eq - *e) + 1);
		memcpy(name, *e, (size_t)(eq - *e));
		name[eq - *e] = '\0';
		mp_set(*ret, name, mp_str(eq + 1));
		free(name);
	}
	return true;
}

static bool o_platform(int argc, Value *argv, Value *ret)
{
	struct utsname u;
	(void)argv;
	MP_CHECK(argc == 0, "platform() takes no arguments.");
	MP_CHECK(uname(&u) == 0, "uname failed.");
	for (char *c = u.sysname; *c; c++)
		*c = (char)tolower((unsigned char)*c);
	*ret = mp_str(u.sysname);
	return true;
}

static bool o_arch(int argc, Value *argv, Value *ret)
{
	struct utsname u;
	(void)argv;
	MP_CHECK(argc == 0, "arch() takes no arguments.");
	MP_CHECK(uname(&u) == 0, "uname failed.");
	*ret = mp_str(u.machine);
	return true;
}

static bool o_hostname(int argc, Value *argv, Value *ret)
{
	char buf[256];
	(void)argv;
	MP_CHECK(argc == 0, "hostname() takes no arguments.");
	MP_CHECK(gethostname(buf, sizeof buf) == 0, "gethostname failed.");
	buf[sizeof buf - 1] = '\0';
	*ret = mp_str(buf);
	return true;
}

static bool o_user(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "user() takes no arguments.");
	struct passwd *pw = getpwuid(getuid());
	*ret = pw ? mp_str(pw->pw_name) : mp_none();
	return true;
}

static bool o_pid(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "pid() takes no arguments.");
	*ret = mp_int(getpid());
	return true;
}

static bool o_exit(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc <= 1 && (argc == 0 || mp_is_int(argv[0])), "exit(code) takes an optional integer.");
	fflush(stdout);
	exit(argc ? (int)mp_to_int(argv[0]) : 0);
	return true;
}

MP_MODULE({"env", o_env}, {"setenv", o_setenv}, {"unsetenv", o_unsetenv}, {"environ", o_environ},
	{"platform", o_platform}, {"arch", o_arch}, {"hostname", o_hostname}, {"user", o_user},
	{"pid", o_pid}, {"exit", o_exit});
