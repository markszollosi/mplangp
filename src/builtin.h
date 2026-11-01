#ifndef BUILTIN_H
#define BUILTIN_H

#include "table.h"

void builtin_set_args(int argc, char **argv); /* arguments after the script path */
void builtin_install(Table *globals); /* binds len, push, keys, ... as globals */

#endif
