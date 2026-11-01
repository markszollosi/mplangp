#include <ctype.h>
#include "mplangpfunc.h"

static bool s_len(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_STRING)
		return mp_error(ret, "len(s) takes one string.");
	*ret = mp_int((int64_t)strlen(argv[0].as.s));
	return true;
}

static bool s_upper(int argc, Value *argv, Value *ret)
{
	if (argc != 1 || argv[0].type != VAL_STRING)
		return mp_error(ret, "upper(s) takes one string.");
	*ret = mp_string(argv[0].as.s, (int)strlen(argv[0].as.s));
	for (char *c = ret->as.s; *c; c++)
		*c = (char)toupper((unsigned char)*c);
	return true;
}

MP_MODULE({"len", s_len}, {"upper", s_upper});
