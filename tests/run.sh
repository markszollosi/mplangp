#!/bin/sh
dir=$(dirname "$0")
bin=${MPLANGP:-"$dir/../mplangp"}
out=$(mktemp) || exit 1
trap 'rm -f "$out"' EXIT
fail=0
for src in "$dir"/samples/*.my; do
	name=$(basename "$src" .my)
	if "$bin" "$src" > "$out" 2>/dev/null && diff "$out" "${src%.my}.expected" > /dev/null; then
		echo "PASS $name"
	else
		echo "FAIL $name"
		fail=1
	fi
done
exit $fail
