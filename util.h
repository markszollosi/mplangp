#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
void die(const char *fmt, ...);

#endif
