#!/bin/sh
# testcache.sh [-c] [-i] key|hit|mark|run
# Caches a passing `make test` by tree hash: HEAD tree plus working-tree diff
# and untracked files under src modules tests docs Makefile. -c ignores the
# working tree (committed HEAD only). -i uses the stricter CI marker
# (no warnings); a CI pass also marks the plain one.
P="src modules tests docs Makefile"
ci=
com=
while getopts ci o; do
	case $o in
	c) com=1 ;;
	i) ci=.ci ;;
	*) exit 2 ;;
	esac
done
shift $((OPTIND - 1))
top=$(git rev-parse --show-toplevel 2>/dev/null) || exit 2
cd "$top" || exit 2
dir=${TMPDIR:-/tmp}/mplangp-testcache-$(printf '%s' "$top" | cksum | cut -d' ' -f1)
tree=$(git rev-parse 'HEAD^{tree}') || exit 2
extra=
if [ -z "$com" ]; then
	extra=$(
		git diff HEAD -- $P
		git ls-files -o --exclude-standard -- $P | while IFS= read -r f; do
			printf '%s\n' "$f"
			cat "$f"
		done
	)
fi
if [ -n "$extra" ]; then
	key=$(printf '%s\n%s' "$tree" "$extra" | git hash-object --stdin)
else
	key=$tree
fi
case "$1" in
key) echo "$key" ;;
hit) [ -f "$dir/$key$ci" ] ;;
mark)
	mkdir -p "$dir" && : >"$dir/$key$ci" && : >"$dir/$key"
	;;
run)
	[ -f "$dir/$key" ] && { echo "tests cached ($key)"; exit 0; }
	make test || exit 1
	mkdir -p "$dir" && : >"$dir/$key"
	;;
*) exit 2 ;;
esac
