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
check 70 "Stack overflow: calls nested too deeply" 'def r(n):\n    return r(n + 1)\nr(0)\n'
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
check 70 "line 2: Stack overflow: calls nested too deeply (limit 9999)." 'def r(n):\n    return r(n + 1)\nr(0)\n'
check 70 "Key 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' not found." 'm = {}\nm["aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"]\n'
gen() {
	awk "BEGIN { $1 }" > "$src"
	"$bin" "$src" > /dev/null 2> "$err"
	code=$?
	if [ "$code" = "$2" ] && { [ -z "$3" ] || grep -qF "$3" "$err"; }; then
		echo "PASS limit: $3"
	else
		echo "FAIL limit: $3 (exit $code)"
		fail=1
	fi
}
gen 'for (i = 0; i < 65537; i++) printf "x = %d\n", i' 65 "line 65536: too many constants"
gen 'print "def f():"; print "    if 1:"; print "        x = 1"; for (i = 0; i < 25000; i++) print "        print(1 + 2 + 3)"; print "    else:"; print "        x = 2"' 65 "jump too large"
gen 'print "while True:"; for (i = 0; i < 25000; i++) print "    print(1 + 2 + 3)"' 65 "loop body too large"
gen 'printf "f("; for (i = 0; i < 256; i++) printf "%s%d", i ? "," : "", i; print ")"' 65 "line 1: too many arguments"
gen 'printf "x = ["; for (i = 0; i < 256; i++) printf "%s%d", i ? "," : "", i; print "]"' 65 "line 1: list literal too long"
gen 'printf "x = {"; for (i = 0; i < 256; i++) printf "%s%d: 1", i ? "," : "", i; print "}"' 65 "line 1: map literal too long"
gen 'print "def f():"; for (i = 0; i < 256; i++) printf "    v%d = 1\n", i' 65 "too many local variables"
gen 'for (i = 0; i < 17; i++) { for (j = 0; j < i; j++) printf "    "; print "for x in [1]:" } for (j = 0; j < 17; j++) printf "    "; print "pass"' 65 "for loops nested too deeply"
gen 'for (i = 0; i < 33; i++) { for (j = 0; j < i; j++) printf "    "; print "while True:" } for (j = 0; j < 33; j++) printf "    "; print "break"' 65 "loops nested too deeply"
gen 'print "while True:"; for (i = 0; i < 1000; i++) print "    if 1:\n        break"; print "    break"' 0 ""
gen 'printf "x = "; for (i = 0; i < 100; i++) printf "1"; print ""' 65 "number too long"
deep=$(awk 'BEGIN { printf "x = "; for (i = 0; i < 200; i++) printf "("; printf "1"; for (i = 0; i < 200; i++) printf ")"; print "" }')
check 65 "line 1: nesting too deep" "$deep\n"
deep=$(awk 'BEGIN { printf "x = "; for (i = 0; i < 200; i++) printf "["; for (i = 0; i < 200; i++) printf "]"; print "" }')
check 65 "line 1: nesting too deep" "$deep\n"
deep=$(awk 'BEGIN { printf "x = "; for (i = 0; i < 200; i++) printf "-"; print "1" }')
check 65 "line 1: nesting too deep" "$deep\n"
deep=$(awk 'BEGIN { for (i = 0; i < 130; i++) { for (j = 0; j < i; j++) printf " "; print "if true:" } for (j = 0; j < 130; j++) printf " "; print "x = 1" }')
check 65 "nesting too deep" "$deep\n"
check 70 "Integer overflow." 'print(9223372036854775807 + 1)\n'
check 70 "Integer overflow." 'print(-9223372036854775807 - 2)\n'
check 70 "Integer overflow." 'print(4294967296 * 4294967296)\n'
check 70 "Integer overflow." 'x = -9223372036854775807 - 1\nprint(-x)\n'
check 70 "String too long." 's = "ab" * 1000\nt = s * 1000000\n'
check 70 "range is too large." 'range(-9000000000000000000, 9000000000000000000)\n'
# unit2: loader robustness
if [ "$("$bin" /nonexistent/x.my 2>&1)" = "mplangp: cannot open '/nonexistent/x.my'" ]; then
	echo "PASS missing script"
else
	echo "FAIL missing script"
	fail=1
fi
if [ "$("$bin" /tmp 2>&1)" = "mplangp: cannot read '/tmp'" ]; then
	echo "PASS directory as script"
else
	echo "FAIL directory as script"
	fail=1
fi
check 70 "No module named" 'import nosuchmod\n'
exit $fail
