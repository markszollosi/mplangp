#!/bin/sh
# usage: tools/bless.sh [-y] <sample>   show diff vs .expected; write it only with -y.
yes=0
if [ "$1" = -y ]; then yes=1; shift; fi
[ $# -eq 1 ] || { echo "usage: $0 [-y] <sample>" >&2; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
src=$1
[ -f "$src" ] || src="$root/tests/samples/${1%.my}.my"
[ -f "$src" ] || { echo "no such sample: $1" >&2; exit 1; }
exp=${src%.my}.expected
MPLANGP_MODULES="$root/modules"
export MPLANGP_MODULES
out=$(mktemp) || exit 1
trap 'rm -f "$out"' EXIT
"${MPLANGP:-$root/mplangp}" "$src" > "$out" || echo "warning: exit status $?" >&2
if [ -f "$exp" ]; then
	if diff -u "$exp" "$out"; then echo "unchanged"; exit 0; fi
else
	echo "new $exp:"
	cat "$out"
fi
if [ $yes -eq 1 ]; then
	cp "$out" "$exp" && echo "blessed $exp"
else
	echo "not written; rerun with -y to bless"
fi
