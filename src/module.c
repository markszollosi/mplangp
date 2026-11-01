#include <ctype.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "util.h"

#define PATHS_MAX 16
#define MODULES_MAX 64

static char *paths[PATHS_MAX];
static int npaths;
static Module *loaded[MODULES_MAX];
static int nloaded;

void module_add_path(const char *dir)
{
	if (npaths >= PATHS_MAX)
		return;
	size_t n = strlen(dir) + 1;
	paths[npaths] = xmalloc(n);
	memcpy(paths[npaths++], dir, n);
}

static bool valid_name(const char *name)
{
	if (!*name)
		return false;
	for (; *name; name++)
		if (!isalnum((unsigned char)*name) && *name != '_')
			return false;
	return true;
}

static Module *open_at(const char *path, const char *name, char *err, int errlen)
{
	void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (!h) {
		const char *why = dlerror();
		snprintf(err, errlen, "Cannot load module '%s': %s", name, why ? why : "unknown error");
		return NULL;
	}
	const int *abi = dlsym(h, "mplangp_abi");
	const MpEntry *entries = dlsym(h, "mplangp_entries");
	if (!abi || !entries || *abi != MP_ABI) {
		snprintf(err, errlen, "Module '%s' is not a compatible mplangp module.", name);
		dlclose(h);
		return NULL;
	}
	Module *m = xmalloc(sizeof *m);
	m->name = xmalloc(strlen(name) + 1);
	strcpy(m->name, name);
	m->handle = h;
	m->entries = entries;
	return m;
}

Module *module_load(const char *name, char *err, int errlen)
{
	for (int i = 0; i < nloaded; i++)
		if (strcmp(loaded[i]->name, name) == 0)
			return loaded[i];
	if (!valid_name(name) || nloaded >= MODULES_MAX) {
		snprintf(err, errlen, "No module named '%s'.", name);
		return NULL;
	}
	for (int i = 0; i < npaths; i++) {
		char path[1024];
		snprintf(path, sizeof path, "%s/%s.so", paths[i], name);
		FILE *f = fopen(path, "rb");
		if (!f)
			continue;
		fclose(f);
		Module *m = open_at(path, name, err, errlen);
		if (m)
			loaded[nloaded++] = m;
		return m;
	}
	snprintf(err, errlen, "No module named '%s'.", name);
	return NULL;
}

bool module_lookup(Module *m, const char *name, Value *out)
{
	for (const MpEntry *e = m->entries; e->name; e++) {
		if (strcmp(e->name, name) == 0) {
			out->type = VAL_NATIVE;
			out->as.native = e;
			return true;
		}
	}
	return false;
}

void module_free_all(void)
{
	for (int i = 0; i < nloaded; i++) {
		dlclose(loaded[i]->handle);
		free(loaded[i]->name);
		free(loaded[i]);
	}
	nloaded = 0;
	for (int i = 0; i < npaths; i++)
		free(paths[i]);
	npaths = 0;
}
