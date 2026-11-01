#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void *xmalloc(size_t size);
void *xrealloc(void *p, size_t size);
void die(const char *fmt, ...);

#endif
