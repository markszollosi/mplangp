#!/bin/sh
# usage: structure.sh [map]   map: only check files against filemap.txt
cd "$(dirname "$0")/.." || exit 1
map=.claude/filemap.txt
fail=0
bad()
{
	echo "structure: $1"
	fail=1
}
mapped()
{
	d=${1%/*}
	b=${1##*/}
	n=${b%.*}
	e=${b##*.}
	grep -q -F -e "$1" -e "$d/*.$e" "$map" && return 0
	grep -q -F "$d/$n.[" "$map" && return 0
	grep -q -E "(^|[ /])$b( |$)" "$map" && return 0
	case $b in test_*.c) grep -q -F "$d/test_<x>.c" "$map" && return 0;; esac
	return 1
}
for f in src/*.c src/*.h modules/*.c tests/*.c tests/*.sh; do
	[ -e "$f" ] || continue
	mapped "$f" || bad "$f missing from $map"
done
[ "$1" = map ] && exit $fail
for f in src/*.c; do
	b=${f#src/}
	[ "$b" = main.c ] && continue
	grep -q "^SRC = \(.* \)\{0,1\}$b\( \|$\)" Makefile || bad "$f missing from SRC in Makefile"
done
for f in tests/test_*.c; do
	t=${f#tests/test_}
	t=${t%.c}
	grep -q "^TESTS = \(.* \)\{0,1\}$t\( \|$\)" Makefile || bad "$f missing from TESTS in Makefile"
	grep -q "^T_$t " Makefile || bad "$f has no T_$t in Makefile"
done
[ $fail = 0 ] && echo "PASS structure"
exit $fail
