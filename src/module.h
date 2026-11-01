#ifndef MODULE_H
#define MODULE_H

#include "mplangpfunc.h"

typedef struct Module {
	char *name;
	void *handle;
	const MpEntry *entries;
} Module;

void module_add_path(const char *dir);
Module *module_load(const char *name, char *err, int errlen); /* NULL + message in err */
bool module_lookup(Module *m, const char *name, Value *out);  /* native, borrowed */
void module_free_all(void);

#endif
