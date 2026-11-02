#!/bin/sh
# usage: tools/new-src.sh name   (creates src/name.[ch], tests/test_name.c, Makefile + filemap entries)
root=$(cd "$(dirname "$0")/.." && pwd)
n=$1
case "$n" in
'' | *[!a-z0-9_]* | [0-9]*) echo "usage: $0 name (lowercase letters, digits, _)" >&2; exit 2 ;;
esac
cd "$root" || exit 1
[ -e "src/$n.c" ] && { echo "src/$n.c exists" >&2; exit 1; }
g=$(printf '%s' "$n" | tr a-z A-Z)_H
cat > "src/$n.h" <<EOT
#ifndef $g
#define $g

int ${n}_init(void);

#endif
EOT
cat > "src/$n.c" <<EOT
#include "$n.h"

int ${n}_init(void)
{
	return 0;
}
EOT
cat > "tests/test_$n.c" <<EOT
#include <assert.h>
#include <stdio.h>
#include "$n.h"

int main(void)
{
	assert(${n}_init() == 0);
	puts("$n ok");
	return 0;
}
EOT
awk -v n="$n" '
	/^SRC = / { $0 = $0 " " n ".c" }
	/^TESTS = / { $0 = $0 " " n }
	{ print }
	/^T_vm / { printf "T_%-8s = $(CORE) src/%s.c\n", n, n }
' Makefile > Makefile.new && mv Makefile.new Makefile
awk -v n="$n" '
	{ print }
	/^src\/util\.\[ch\]/ { printf "%-17s TODO one-line purpose\n", "src/" n ".[ch]" }
' .claude/filemap.txt > .claude/filemap.new && mv .claude/filemap.new .claude/filemap.txt
echo "created src/$n.[ch] tests/test_$n.c; adjust T_$n in Makefile if it links other sources, fill in the filemap line"
