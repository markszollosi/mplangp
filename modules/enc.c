#include <ctype.h>
#include "mplangpfunc.h"

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool e_b64(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "b64(text) takes a string.");
	const unsigned char *s = (const unsigned char *)argv[0].as.s;
	size_t n = strlen(argv[0].as.s);
	char *out = mp_alloc((n + 2) / 3 * 4 + 1);
	size_t o = 0;
	for (size_t i = 0; i < n; i += 3) {
		unsigned v = (unsigned)s[i] << 16;
		if (i + 1 < n)
			v |= (unsigned)s[i + 1] << 8;
		if (i + 2 < n)
			v |= s[i + 2];
		out[o++] = B64[(v >> 18) & 63];
		out[o++] = B64[(v >> 12) & 63];
		out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
		out[o++] = i + 2 < n ? B64[v & 63] : '=';
	}
	out[o] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

static int b64val(char c)
{
	const char *p = c ? strchr(B64, c) : NULL;
	if (p)
		return (int)(p - B64);
	if (c == '-')
		return 62;
	if (c == '_')
		return 63;
	return -1;
}

static bool e_unb64(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "unb64(text) takes a string.");
	const char *s = argv[0].as.s;
	char *out = mp_alloc(strlen(s) / 4 * 3 + 4);
	size_t o = 0;
	unsigned acc = 0;
	int bits = 0;
	for (; *s && *s != '='; s++) {
		if (isspace((unsigned char)*s))
			continue;
		int v = b64val(*s);
		if (v < 0) {
			free(out);
			return mp_error(ret, "unb64: invalid base64 character.");
		}
		acc = (acc << 6) | (unsigned)v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out[o++] = (char)((acc >> bits) & 0xFF);
		}
	}
	out[o] = '\0';
	if (strlen(out) != o) {
		free(out);
		return mp_error(ret, "unb64: decoded data contains NUL bytes.");
	}
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool e_hex(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "hex(text) takes a string.");
	size_t n = strlen(argv[0].as.s);
	char *out = mp_alloc(n * 2 + 1);
	for (size_t i = 0; i < n; i++)
		sprintf(out + i * 2, "%02x", (unsigned char)argv[0].as.s[i]);
	*ret = mp_str(out);
	free(out);
	return true;
}

static int hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool e_unhex(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "unhex(text) takes a string.");
	const char *s = argv[0].as.s;
	size_t n = strlen(s);
	MP_CHECK(n % 2 == 0, "unhex: odd number of digits.");
	char *out = mp_alloc(n / 2 + 1);
	for (size_t i = 0; i < n; i += 2) {
		int a = hexval(s[i]), b = hexval(s[i + 1]);
		if (a < 0 || b < 0 || (a == 0 && b == 0)) {
			free(out);
			return mp_error(ret, a < 0 || b < 0 ? "unhex: invalid hex digit." : "unhex: decoded data contains NUL bytes.");
		}
		out[i / 2] = (char)(a * 16 + b);
	}
	out[n / 2] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool e_url(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "url(text) takes a string.");
	const char *s = argv[0].as.s;
	char *out = mp_alloc(strlen(s) * 3 + 1);
	size_t o = 0;
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			out[o++] = (char)c;
		else
			o += (size_t)sprintf(out + o, "%%%02X", c);
	}
	out[o] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

static bool e_unurl(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "unurl(text) takes a string.");
	const char *s = argv[0].as.s;
	char *out = mp_alloc(strlen(s) + 1);
	size_t o = 0;
	for (; *s; s++) {
		if (*s == '%' && hexval(s[1]) >= 0 && hexval(s[2]) >= 0) {
			int v = hexval(s[1]) * 16 + hexval(s[2]);
			if (v == 0) {
				free(out);
				return mp_error(ret, "unurl: decoded data contains NUL bytes.");
			}
			out[o++] = (char)v;
			s += 2;
		} else {
			out[o++] = *s == '+' ? ' ' : *s;
		}
	}
	out[o] = '\0';
	*ret = mp_str(out);
	free(out);
	return true;
}

MP_MODULE({"b64", e_b64}, {"unb64", e_unb64}, {"hex", e_hex}, {"unhex", e_unhex},
	{"url", e_url}, {"unurl", e_unurl});
