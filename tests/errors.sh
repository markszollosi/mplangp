#!/bin/sh
# each case: expected exit code, expected stderr message, program
dir=$(dirname "$0")
bin=${MPLANGP:-"$dir/../mplangp"}
MPLANGP_MODULES="$dir/../modules"
export MPLANGP_MODULES
src=$(mktemp) || exit 1
err=$(mktemp) || exit 1
trap 'rm -f "$src" "$err"' EXIT
fail=0
check() {
	printf '%b' "$3" > "$src"
	"$bin" "$src" > /dev/null 2> "$err"
	code=$?
	if [ "$code" = "$1" ] && grep -qF "$2" "$err"; then
		echo "PASS error: $2"
	else
		echo "FAIL error: $2 (exit $code)"
		fail=1
	fi
}
check 70 "Expected 1 arguments but got 2." 'def f(a):\n    return a\nf(1, 2)\n'
check 70 "No module named 'nope'." 'import nope\n'
check 70 "has no attribute 'nothing'" 'import math\nmath.nothing\n'
check 70 "sqrt of a negative number." 'import math\nmath.sqrt(-1)\n'
check 70 "Can only call functions." 'x = 5\nx()\n'
check 70 "Stack overflow." 'def r(n):\n    return r(n + 1)\nr(0)\n'
check 65 "nested def is not supported" 'def f():\n    def g():\n        return 1\n'
check 65 "'return' outside function" 'return 1\n'
check 65 "duplicate parameter" 'def f(a, a):\n    return a\n'
check 65 "'break' outside loop" 'break\n'
check 65 "'continue' outside loop" 'if True:\n    continue\n'
check 65 "'break' outside loop" 'while True:\n    def f():\n        break\n    break\n'
exit $fail
