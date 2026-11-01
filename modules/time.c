#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "mplangpfunc.h"

static double now(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static bool t_now(int argc, Value *argv, Value *ret)
{
	(void)argv;
	MP_CHECK(argc == 0, "now() takes no arguments.");
	*ret = mp_float(now());
	return true;
}

static bool t_mono(int argc, Value *argv, Value *ret)
{
	struct timespec ts;
	(void)argv;
	MP_CHECK(argc == 0, "mono() takes no arguments.");
	clock_gettime(CLOCK_MONOTONIC, &ts);
	*ret = mp_float((double)ts.tv_sec + (double)ts.tv_nsec / 1e9);
	return true;
}

static bool t_sleep(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_num(argv[0]), "sleep(seconds) takes a number.");
	double s = mp_to_float(argv[0]);
	MP_CHECK(s >= 0 && s < 86400, "sleep(seconds) takes a number from 0 to 86400.");
	struct timespec ts;
	ts.tv_sec = (time_t)s;
	ts.tv_nsec = (long)((s - floor(s)) * 1e9);
	fflush(stdout);
	while (nanosleep(&ts, &ts) != 0)
		;
	return true;
}

static bool get_tm(Value ts, bool utc, struct tm *tm)
{
	time_t t = (time_t)floor(mp_to_float(ts));
	return (utc ? gmtime_r(&t, tm) : localtime_r(&t, tm)) != NULL;
}

static bool t_format(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 3 && mp_is_num(argv[0]), "format(ts, fmt, utc) takes a number, an optional format and an optional bool.");
	MP_CHECK(argc < 2 || mp_is_str(argv[1]), "format(ts, fmt, utc) takes the format as a string.");
	bool utc = argc == 3 && argv[2].type == VAL_BOOL && argv[2].as.b;
	const char *fmt = argc >= 2 ? argv[1].as.s : "%Y-%m-%d %H:%M:%S";
	struct tm tm;
	MP_CHECK(get_tm(argv[0], utc, &tm), "timestamp out of range.");
	char buf[512];
	size_t n = strftime(buf, sizeof buf, fmt, &tm);
	MP_CHECK(n > 0 || !*fmt, "format result is empty or too long.");
	*ret = mp_string(buf, (int)n);
	return true;
}

static bool t_iso(int argc, Value *argv, Value *ret)
{
	struct tm tm;
	char buf[64];
	MP_CHECK(argc == 1 && mp_is_num(argv[0]), "iso(ts) takes a number.");
	MP_CHECK(get_tm(argv[0], true, &tm), "timestamp out of range.");
	strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
	*ret = mp_str(buf);
	return true;
}

static bool t_parse(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 3 && mp_is_str(argv[0]), "parse(text, fmt, utc) takes a string, an optional format and an optional bool.");
	MP_CHECK(argc < 2 || mp_is_str(argv[1]), "parse(text, fmt, utc) takes the format as a string.");
	bool utc = argc == 3 && argv[2].type == VAL_BOOL && argv[2].as.b;
	const char *fmt = argc >= 2 ? argv[1].as.s : "%Y-%m-%d %H:%M:%S";
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	tm.tm_isdst = -1;
	const char *end = strptime(argv[0].as.s, fmt, &tm);
	MP_CHECK(end && !*end, "parse: text does not match the format.");
	time_t t = utc ? timegm(&tm) : mktime(&tm);
	*ret = mp_int((int64_t)t);
	return true;
}

static bool t_parts(int argc, Value *argv, Value *ret)
{
	struct tm tm;
	MP_CHECK(argc >= 1 && argc <= 2 && mp_is_num(argv[0]), "parts(ts, utc) takes a number and an optional bool.");
	bool utc = argc == 2 && argv[1].type == VAL_BOOL && argv[1].as.b;
	MP_CHECK(get_tm(argv[0], utc, &tm), "timestamp out of range.");
	*ret = mp_map();
	mp_set(*ret, "year", mp_int(tm.tm_year + 1900));
	mp_set(*ret, "month", mp_int(tm.tm_mon + 1));
	mp_set(*ret, "day", mp_int(tm.tm_mday));
	mp_set(*ret, "hour", mp_int(tm.tm_hour));
	mp_set(*ret, "minute", mp_int(tm.tm_min));
	mp_set(*ret, "second", mp_int(tm.tm_sec));
	mp_set(*ret, "weekday", mp_int(tm.tm_wday));
	mp_set(*ret, "yearday", mp_int(tm.tm_yday + 1));
	return true;
}

MP_MODULE({"now", t_now}, {"mono", t_mono}, {"sleep", t_sleep}, {"format", t_format},
	{"iso", t_iso}, {"parse", t_parse}, {"parts", t_parts});
