#include <math.h>
#include "mplangpfunc.h"

static bool m_sqrt(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "sqrt(x) takes one number.");
	if (mp_to_float(argv[0]) < 0)
		return mp_error(ret, "sqrt of a negative number.");
	*ret = mp_float(sqrt(mp_to_float(argv[0])));
	return true;
}

static bool m_pow(int argc, Value *argv, Value *ret)
{
	if (argc != 2 || !mp_is_num(argv[0]) || !mp_is_num(argv[1]))
		return mp_error(ret, "pow(x, y) takes two numbers.");
	*ret = mp_float(pow(mp_to_float(argv[0]), mp_to_float(argv[1])));
	return true;
}

static bool m_floor(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "floor(x) takes one number.");
	*ret = mp_int((int64_t)floor(mp_to_float(argv[0])));
	return true;
}

static bool m_abs(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "abs(x) takes one number.");
	if (argv[0].type == VAL_FLOAT)
		*ret = mp_float(fabs(argv[0].as.f));
	else
	{
		long long v = mp_to_int(argv[0]);
		*ret = mp_int(v < 0 ? (long long)(0 - (unsigned long long)v) : v);
	}
	return true;
}

static bool pick(int argc, Value *argv, Value *ret, bool want_max)
{
	if (argc < 1)
		return mp_error(ret, want_max ? "max(a, b, ...) takes numbers." : "min(a, b, ...) takes numbers.");
	int best = 0;
	for (int i = 0; i < argc; i++) {
		if (!mp_is_num(argv[i]))
			return mp_error(ret, want_max ? "max(a, b, ...) takes numbers." : "min(a, b, ...) takes numbers.");
		double a = mp_to_float(argv[i]), b = mp_to_float(argv[best]);
		if (want_max ? a > b : a < b)
			best = i;
	}
	*ret = argv[best].type == VAL_FLOAT ? mp_float(argv[best].as.f) : mp_int(mp_to_int(argv[best]));
	return true;
}

static bool m_min(int argc, Value *argv, Value *ret)
{
	return pick(argc, argv, ret, false);
}

static bool m_max(int argc, Value *argv, Value *ret)
{
	return pick(argc, argv, ret, true);
}

static bool m_ceil(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "ceil(x) takes one number.");
	*ret = mp_int((int64_t)ceil(mp_to_float(argv[0])));
	return true;
}

static bool m_round(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "round(x) takes one number.");
	*ret = mp_int((int64_t)round(mp_to_float(argv[0])));
	return true;
}

#define UNARY(name, expr) \
static bool m_##name(int argc, Value *argv, Value *ret) \
{ \
	if (argc != 1 || !mp_is_num(argv[0])) \
		return mp_error(ret, #name "(x) takes one number."); \
	double x = mp_to_float(argv[0]); \
	*ret = mp_float(expr); \
	return true; \
}

UNARY(sin, sin(x))
UNARY(cos, cos(x))
UNARY(tan, tan(x))
UNARY(exp, exp(x))

static bool m_log(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "log(x) takes one number.");
	if (mp_to_float(argv[0]) <= 0)
		return mp_error(ret, "log of a non-positive number.");
	*ret = mp_float(log(mp_to_float(argv[0])));
	return true;
}

static bool m_log10(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || !mp_is_num(argv[0]))
		return mp_error(ret, "log10(x) takes one number.");
	if (mp_to_float(argv[0]) <= 0)
		return mp_error(ret, "log10 of a non-positive number.");
	*ret = mp_float(log10(mp_to_float(argv[0])));
	return true;
}

static bool m_atan2(int argc, Value *argv, Value *ret)
{
	if (argc != 2 || !mp_is_num(argv[0]) || !mp_is_num(argv[1]))
		return mp_error(ret, "atan2(y, x) takes two numbers.");
	*ret = mp_float(atan2(mp_to_float(argv[0]), mp_to_float(argv[1])));
	return true;
}

static bool m_pi(int argc, Value *argv, Value *ret)
{
	(void)argv;
	if (argc != 0)
		return mp_error(ret, "pi() takes no arguments.");
	*ret = mp_float(3.14159265358979323846);
	return true;
}

MP_MODULE({"sqrt", m_sqrt}, {"pow", m_pow}, {"floor", m_floor}, {"abs", m_abs},
	{"min", m_min}, {"max", m_max}, {"ceil", m_ceil}, {"round", m_round},
	{"sin", m_sin}, {"cos", m_cos}, {"tan", m_tan}, {"exp", m_exp}, {"log", m_log},
	{"log10", m_log10}, {"atan2", m_atan2}, {"pi", m_pi});
