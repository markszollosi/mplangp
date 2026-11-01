#include <ctype.h>
#include <curl/curl.h>
#include "mplangpfunc.h"

#define BODY_MAX (64 * 1024 * 1024)

static double timeout_s = 30.0;
static bool inited;

typedef struct {
	char *s;
	size_t n, cap;
	bool over;
} Buf;

static size_t on_body(char *p, size_t size, size_t nmemb, void *ud)
{
	Buf *b = ud;
	size_t n = size * nmemb;
	if (b->n + n > BODY_MAX) {
		b->over = true;
		return 0;
	}
	if (b->n + n + 1 > b->cap) {
		size_t cap = (b->n + n + 1) * 2;
		char *s = mp_alloc(cap);
		if (b->n)
			memcpy(s, b->s, b->n);
		free(b->s);
		b->s = s;
		b->cap = cap;
	}
	memcpy(b->s + b->n, p, n);
	b->n += n;
	return n;
}

static size_t on_header(char *p, size_t size, size_t nmemb, void *ud)
{
	Value *headers = ud;
	size_t n = size * nmemb;
	const char *colon = memchr(p, ':', n);
	if (!colon)
		return n;
	char name[128];
	size_t nl = (size_t)(colon - p);
	if (nl == 0 || nl >= sizeof name)
		return n;
	for (size_t i = 0; i < nl; i++)
		name[i] = (char)tolower((unsigned char)p[i]);
	name[nl] = '\0';
	const char *v = colon + 1, *end = p + n;
	while (v < end && (*v == ' ' || *v == '\t'))
		v++;
	while (end > v && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' '))
		end--;
	mp_set(*headers, name, mp_string_clean(v, (size_t)(end - v)));
	return n;
}

static bool http_ready(Value *ret)
{
	if (!inited && curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
		return mp_error(ret, "cannot initialise libcurl.");
	inited = true;
	return true;
}

static bool valid_url(Value v)
{
	return mp_is_str(v) && (strncmp(v.as.s, "http://", 7) == 0 || strncmp(v.as.s, "https://", 8) == 0);
}

/* Performs a request; body may be NULL, file receives the body instead of memory when non-NULL. */
static bool perform(Value *ret, const char *method, const char *url, const char *body, Value headers, FILE *file)
{
	if (!http_ready(ret))
		return false;
	CURL *c = curl_easy_init();
	if (!c)
		return mp_error(ret, "cannot create a request.");
	Buf b = { NULL, 0, 0, false };
	Value resp_headers = mp_map();
	struct curl_slist *list = NULL;
	char errbuf[CURL_ERROR_SIZE] = "";
	if (headers.type == VAL_MAP) {
		for (int i = 0; i < headers.as.map->count; i++) {
			Value v = headers.as.map->vals[i];
			if (!mp_is_str(v)) {
				curl_easy_cleanup(c);
				mp_value_release(resp_headers);
				return mp_error(ret, "header values must be strings.");
			}
			char *line = mp_alloc(strlen(headers.as.map->keys[i]) + strlen(v.as.s) + 3);
			sprintf(line, "%s: %s", headers.as.map->keys[i], v.as.s);
			list = curl_slist_append(list, line);
			free(line);
		}
	}
	curl_easy_setopt(c, CURLOPT_URL, url);
	curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, method);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 10L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, (long)(timeout_s * 1000));
	curl_easy_setopt(c, CURLOPT_USERAGENT, "mplangp");
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, errbuf);
	curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, on_header);
	curl_easy_setopt(c, CURLOPT_HEADERDATA, &resp_headers);
	if (file) {
		curl_easy_setopt(c, CURLOPT_WRITEDATA, file);
	} else {
		curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, on_body);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, &b);
	}
	if (strcmp(method, "HEAD") == 0)
		curl_easy_setopt(c, CURLOPT_NOBODY, 1L);
	if (body) {
		curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
		curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
	}
	if (list)
		curl_easy_setopt(c, CURLOPT_HTTPHEADER, list);
	CURLcode rc = curl_easy_perform(c);
	long status = 0;
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(list);
	curl_easy_cleanup(c);
	if (rc != CURLE_OK) {
		mp_value_release(resp_headers);
		free(b.s);
		return mp_errorf(ret, "request failed: %s", b.over ? "response too large" : errbuf[0] ? errbuf : curl_easy_strerror(rc));
	}
	*ret = mp_map();
	mp_set(*ret, "status", mp_int(status));
	mp_set(*ret, "ok", mp_bool(status >= 200 && status < 300));
	mp_set(*ret, "body", mp_string_clean(b.s ? b.s : "", b.n));
	mp_set(*ret, "headers", resp_headers);
	free(b.s);
	return true;
}

static bool h_get(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 1 && argc <= 2 && valid_url(argv[0]), "get(url, headers) takes an http(s) url and optional headers.");
	MP_CHECK(argc == 1 || argv[1].type == VAL_MAP || argv[1].type == VAL_NONE, "get(url, headers) takes headers as a map.");
	return perform(ret, "GET", argv[0].as.s, NULL, argc == 2 ? argv[1] : mp_none(), NULL);
}

static bool h_post(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 2 && argc <= 3 && valid_url(argv[0]) && mp_is_str(argv[1]), "post(url, body, headers) takes an http(s) url, a string and optional headers.");
	MP_CHECK(argc == 2 || argv[2].type == VAL_MAP || argv[2].type == VAL_NONE, "post(url, body, headers) takes headers as a map.");
	return perform(ret, "POST", argv[0].as.s, argv[1].as.s, argc == 3 ? argv[2] : mp_none(), NULL);
}

static bool h_request(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc >= 2 && argc <= 4 && mp_is_str(argv[0]) && valid_url(argv[1]), "request(method, url, body, headers) takes a method, an http(s) url, an optional body and optional headers.");
	MP_CHECK(argc < 3 || mp_is_str(argv[2]) || argv[2].type == VAL_NONE, "request: the body is a string or None.");
	MP_CHECK(argc < 4 || argv[3].type == VAL_MAP || argv[3].type == VAL_NONE, "request: headers are a map.");
	for (const char *m = argv[0].as.s; *m; m++)
		MP_CHECK(isupper((unsigned char)*m), "request: the method must be upper case, like GET.");
	MP_CHECK(argv[0].as.s[0], "request: the method is empty.");
	const char *body = argc >= 3 && mp_is_str(argv[2]) ? argv[2].as.s : NULL;
	return perform(ret, argv[0].as.s, argv[1].as.s, body, argc == 4 ? argv[3] : mp_none(), NULL);
}

static bool h_download(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 2 && valid_url(argv[0]) && mp_is_str(argv[1]), "download(url, path) takes an http(s) url and a path.");
	FILE *f = fopen(argv[1].as.s, "wb");
	if (!f)
		return mp_errorf(ret, "cannot write '%s'.", argv[1].as.s);
	bool ok = perform(ret, "GET", argv[0].as.s, NULL, mp_none(), f);
	fclose(f);
	if (!ok)
		return false;
	Value *status = mp_get(*ret, "status");
	int64_t code = status ? status->as.i : 0;
	mp_value_release(*ret);
	*ret = mp_int(code);
	return true;
}

static bool h_timeout(int argc, Value *argv, Value *ret)
{
	MP_CHECK(argc == 1 && mp_is_num(argv[0]), "timeout(seconds) takes a number.");
	double s = mp_to_float(argv[0]);
	MP_CHECK(s > 0 && s <= 3600, "timeout(seconds) takes a number above 0 up to 3600.");
	timeout_s = s;
	return true;
}

MP_MODULE({"get", h_get}, {"post", h_post}, {"request", h_request}, {"download", h_download},
	{"timeout", h_timeout});
