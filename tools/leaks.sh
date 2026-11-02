#!/bin/sh
# Run every sample under a leak checker; fail on leaks. Not part of make test.
root=$(cd "$(dirname "$0")/.." && pwd)
bin=$root/mplangp
MPLANGP_MODULES=$root/modules
export MPLANGP_MODULES
if command -v leaks >/dev/null 2>&1 && [ "$(uname)" = Darwin ]; then
	mode=leaks
elif command -v valgrind >/dev/null 2>&1; then
	mode=valgrind
else
	echo "leaks: skipped (needs macOS leaks or valgrind)"
	exit 0
fi
out=$(mktemp) || exit 1
trap 'rm -f "$out"' EXIT
fail=0
for src in "$root"/tests/samples/*.my; do
	name=$(basename "$src" .my)
	case $mode in
	leaks)
		leaks --atExit -- "$bin" "$src" > "$out" 2>&1
		grep -q ' 0 leaks for 0 total leaked bytes' "$out" ;;
	valgrind)
		valgrind -q --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=99 "$bin" "$src" > "$out" 2>&1
		[ $? -ne 99 ] ;;
	esac
	if [ $? -eq 0 ]; then
		echo "CLEAN $name"
	else
		echo "LEAK $name"
		grep -E 'leaks for|Leak:|definitely lost' "$out" | head -5
		fail=1
	fi
done
exit $fail
