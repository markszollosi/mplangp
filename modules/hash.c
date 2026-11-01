#include <stdint.h>
#include "mplangpfunc.h"

typedef struct {
	uint32_t h[8];
	uint8_t buf[64];
	uint64_t len;
	size_t fill;
} Sha;

static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void block(Sha *s, const uint8_t *p)
{
	uint32_t w[64], a, b, c, d, e, f, g, h;
	for (int i = 0; i < 16; i++)
		w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
	for (int i = 16; i < 64; i++) {
		uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
	e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
	for (int i = 0; i < 64; i++) {
		uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
		uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
	s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void sha_init(Sha *s)
{
	static const uint32_t init[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
	};
	memcpy(s->h, init, sizeof init);
	s->len = 0;
	s->fill = 0;
}

static void sha_add(Sha *s, const uint8_t *p, size_t n)
{
	s->len += n;
	while (n > 0) {
		size_t take = 64 - s->fill < n ? 64 - s->fill : n;
		memcpy(s->buf + s->fill, p, take);
		s->fill += take;
		p += take;
		n -= take;
		if (s->fill == 64) {
			block(s, s->buf);
			s->fill = 0;
		}
	}
}

static void sha_end(Sha *s, uint8_t out[32])
{
	uint64_t bits = s->len * 8;
	uint8_t pad = 0x80;
	sha_add(s, &pad, 1);
	pad = 0;
	while (s->fill != 56)
		sha_add(s, &pad, 1);
	uint8_t lenb[8];
	for (int i = 0; i < 8; i++)
		lenb[i] = (uint8_t)(bits >> (56 - 8 * i));
	sha_add(s, lenb, 8);
	for (int i = 0; i < 8; i++) {
		out[i * 4] = (uint8_t)(s->h[i] >> 24);
		out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
		out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
		out[i * 4 + 3] = (uint8_t)s->h[i];
	}
}

static Value hexof(const uint8_t *d, size_t n)
{
	char out[65];
	for (size_t i = 0; i < n; i++)
		sprintf(out + i * 2, "%02x", d[i]);
	return mp_str(out);
}

static bool h_sha256(int argc, Value *argv, Value *ret)
{
	Sha s;
	uint8_t d[32];
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "sha256(text) takes a string.");
	sha_init(&s);
	sha_add(&s, (const uint8_t *)argv[0].as.s, strlen(argv[0].as.s));
	sha_end(&s, d);
	*ret = hexof(d, 32);
	return true;
}

static bool h_file(int argc, Value *argv, Value *ret)
{
	Sha s;
	uint8_t d[32], buf[8192];
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "file(path) takes a string.");
	FILE *f = fopen(argv[0].as.s, "rb");
	if (!f)
		return mp_errorf(ret, "cannot read '%s'.", argv[0].as.s);
	sha_init(&s);
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0)
		sha_add(&s, buf, n);
	bool bad = ferror(f);
	fclose(f);
	MP_CHECK(!bad, "file(path) failed while reading.");
	sha_end(&s, d);
	*ret = hexof(d, 32);
	return true;
}

static bool h_hmac(int argc, Value *argv, Value *ret)
{
	Sha s;
	uint8_t k[64] = { 0 }, ipad[64], opad[64], inner[32], d[32];
	MP_CHECK(argc == 2 && mp_is_str(argv[0]) && mp_is_str(argv[1]), "hmac(key, text) takes two strings.");
	size_t klen = strlen(argv[0].as.s);
	if (klen > 64) {
		sha_init(&s);
		sha_add(&s, (const uint8_t *)argv[0].as.s, klen);
		sha_end(&s, k);
	} else {
		memcpy(k, argv[0].as.s, klen);
	}
	for (int i = 0; i < 64; i++) {
		ipad[i] = k[i] ^ 0x36;
		opad[i] = k[i] ^ 0x5c;
	}
	sha_init(&s);
	sha_add(&s, ipad, 64);
	sha_add(&s, (const uint8_t *)argv[1].as.s, strlen(argv[1].as.s));
	sha_end(&s, inner);
	sha_init(&s);
	sha_add(&s, opad, 64);
	sha_add(&s, inner, 32);
	sha_end(&s, d);
	*ret = hexof(d, 32);
	return true;
}

static bool h_crc32(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_str(argv[0]), "crc32(text) takes a string.");
	uint32_t crc = 0xFFFFFFFFu;
	for (const unsigned char *p = (const unsigned char *)argv[0].as.s; *p; p++) {
		crc ^= *p;
		for (int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
	}
	*ret = mp_int((int64_t)(crc ^ 0xFFFFFFFFu));
	return true;
}

MP_MODULE({"sha256", h_sha256}, {"file", h_file}, {"hmac", h_hmac}, {"crc32", h_crc32});
