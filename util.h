#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
void die(const char *fmt, ...);

#endif
