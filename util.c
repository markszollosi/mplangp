#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		die("xmalloc: out of memory (%zu bytes)", size);
	return ptr;
}

void *xrealloc(void *ptr, size_t size)
{
	void *result = realloc(ptr, size);
	if (result == NULL && size != 0)
		die("xrealloc: out of memory (%zu bytes)", size);
	return result;
}

void die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}
