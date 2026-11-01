#!/bin/sh
# Runs every fenced example in docs/**/*.md.
#   ```my          must exit 0; a directly following ```output block must equal stdout
#   ```my-error    must exit non-zero; a following ```output block must equal stderr
#   ```my-norun    shown only (network, clipboard, interactive input, ...)
dir=$(dirname "$0")
root=$(cd "$dir/.." && pwd)
bin=${MPLANGP:-"$root/mplangp"}
MPLANGP_MODULES="$root/modules"
export MPLANGP_MODULES
work=$(mktemp -d) || exit 1
trap 'rm -rf "$work"' EXIT
mkdir "$work/run"
fail=0
for doc in "$root"/docs/*.md "$root"/docs/modules/*.md; do
	[ -f "$doc" ] || continue
	name=$(basename "$doc" .md)
	rm -rf "$work/b"
	mkdir "$work/b"
	awk -v out="$work/b" '
		/^```my(-error)?$/ { n++; kind = ($0 ~ /error/) ? "err" : "ok"; f = out "/" n "." kind ".my"; mode = "code"; print NR > (out "/" n ".line"); close(out "/" n ".line"); next }
		/^```output$/ && n > 0 { f = out "/" n ".out"; mode = "out"; next }
		/^```/ { if (mode != "") close(f); mode = ""; next }
		mode != "" { print > f }
	' "$doc"
	for src in "$work"/b/*.my; do
		[ -f "$src" ] || continue
		base=$(basename "$src" .my)
		num=${base%%.*}
		kind=${base##*.}
		line=$(cat "$work/b/$num.line")
		(cd "$work/run" && "$bin" "$src" > "$work/stdout" 2> "$work/stderr" < /dev/null)
		code=$?
		ok=1
		if [ "$kind" = ok ]; then
			[ "$code" = 0 ] || ok=0
			got="$work/stdout"
		else
			[ "$code" != 0 ] || ok=0
			got="$work/stderr"
		fi
		if [ -f "$work/b/$num.out" ]; then
			# error messages are compared after the "line N: " prefix
			if [ "$kind" = err ]; then
				sed 's/^line [0-9]*: //' "$got" > "$work/got2"
				got="$work/got2"
			fi
			diff "$got" "$work/b/$num.out" > "$work/diff" 2>&1 || ok=0
		fi
		if [ "$ok" = 1 ]; then
			echo "PASS doc $name:$line"
		else
			echo "FAIL doc $name:$line (exit $code)"
			cat "$work/diff" 2> /dev/null | head -10
			head -3 "$work/stderr"
			fail=1
		fi
	done
done
# every exported function must be documented as `name(` on its page
undocumented() {
	# $1 = source file, $2 = doc page, $3 = label
	for fn in $(grep -o '{"[a-z0-9_]*", [a-z_0-9]*}' "$1" | sed 's/{"\([a-z0-9_]*\)".*/\1/'); do
		if ! grep -qF "\`$fn(" "$2"; then
			echo "FAIL doc coverage: $3.$fn is not documented in ${2#$root/}"
			fail=1
		fi
	done
}
for src in "$root"/modules/*.c; do
	m=$(basename "$src" .c)
	if [ ! -f "$root/docs/modules/$m.md" ]; then
		echo "FAIL doc coverage: no docs/modules/$m.md"
		fail=1
		continue
	fi
	undocumented "$src" "$root/docs/modules/$m.md" "$m"
	grep -qF "[$m]($m.md)" "$root/docs/modules/README.md" || { echo "FAIL doc coverage: $m missing from docs/modules/README.md"; fail=1; }
done
undocumented "$root/src/builtin.c" "$root/docs/builtins.md" builtin
[ "$fail" = 0 ] && echo "PASS doc coverage"
exit $fail
