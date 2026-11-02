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
check 65 "integer literal too large" 'print(9223372036854775808)\n'
check 65 "'return' outside function" 'return 1\n'
check 65 "duplicate parameter" 'def f(a, a):\n    return a\n'
check 65 "'break' outside loop" 'break\n'
check 65 "'continue' outside loop" 'if True:\n    continue\n'
check 65 "'break' outside loop" 'while True:\n    def f():\n        break\n    break\n'
check 70 "Index out of range." 'xs = [1]\nxs[1]\n'
check 70 "Index out of range." 'xs = [1]\nxs[5] = 2\n'
check 70 "Key 'k' not found." 'm = {}\nm["k"]\n'
check 70 "Map keys must be strings." 'm = {1: 2}\n'
check 70 "Can only loop over lists, maps and strings." 'for x in 5:\n    print(x)\n'
check 70 "Only lists, maps and strings can be indexed." 'x = 5\nx[0]\n'
check 70 "pop from empty list." 'pop([])\n'
check 70 "int(x) cannot parse the string." 'int("abc")\n'
check 65 "expected ']'" 'x = [1, 2\ny = 3\n'
check 65 "expected 'in'" 'for x range(3):\n    print(x)\n'
check 70 "invalid JSON" 'import json\njson.parse("{bad")\n'
check 70 "invalid JSON: trailing characters" 'import json\njson.parse("1 2")\n'
check 70 "cannot encode functions or modules." 'import json\nimport fs\njson.dump(fs.read)\n'
check 70 "cannot read '/nonexistent/nope'" 'import fs\nfs.read("/nonexistent/nope")\n'
check 70 "bad pattern" 'import re\nre.test("(", "x")\n'
check 70 "sql error" 'import db\nh = db.open(":memory:")\ndb.exec(h, "nonsense")\n'
check 70 "statement needs 1 parameters but got 0." 'import db\nh = db.open(":memory:")\ndb.query(h, "select ?", [])\n'
check 70 "needs a non-empty list of strings." 'import proc\nproc.run([])\n'
check 70 "unb64: invalid base64 character." 'import enc\nenc.unb64("!!")\n'
check 70 "color: unknown style" 'import io\nio.color("plaid", "x")\n'
check 70 "takes an http(s) url" 'import http\nhttp.get("file:///etc/passwd")\n'
check 70 "unknown style" 'import io\nio.color("nope", "x")\n'
check 65 "\\x00 is not allowed in strings" 'x = "a\\x00b"\n'
check 70 "fixed(x, decimals) takes decimals from 0 to 15." 'import str\nstr.fixed(1.5, 20)\n'
check 70 "fixed(x, decimals) takes a number and an integer." 'import str\nstr.fixed("a", 2)\n'
check 70 "pad_left(s, width, fill) takes a string" 'import str\nstr.pad_left("a", 3, "ab")\n'
awk 'BEGIN { for (i = 0; i < 300; i++) printf "v%d = \"s%d\"\n", i, i; print "print(v299 + v0)"; print "print(v150)" }' > "$src"
if [ "$("$bin" "$src" 2> /dev/null)" = "s299s0
s150" ]; then
	echo "PASS wide constants (600 names and literals)"
else
	echo "FAIL wide constants (600 names and literals)"
	fail=1
fi
check 70 "count(s, sub) needs a non-empty sub." 'import str\nstr.count("a", "")\n'
check 70 "ord(c) takes a one-byte string." 'import str\nstr.ord("ab")\n'
check 70 "chr(n) takes an integer from 1 to 255." 'import str\nstr.chr(0)\n'
exit $fail
