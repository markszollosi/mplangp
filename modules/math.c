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
		*ret = mp_int(mp_to_int(argv[0]) < 0 ? -mp_to_int(argv[0]) : mp_to_int(argv[0]));
	return true;
}

MP_MODULE({"sqrt", m_sqrt}, {"pow", m_pow}, {"floor", m_floor}, {"abs", m_abs});
