#!/bin/sh
# PostToolUse: rebuild after a C edit; report only warnings/errors.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in
*/src/*.[ch]) root=${f%/src/*} ;;
*/modules/*.c) root=${f%/modules/*} ;;
*) exit 0 ;;
esac
stamp=
k=
if [ -x "$root/tools/testcache.sh" ]; then
	k=$(cd "$root" && tools/testcache.sh key 2>/dev/null)
	stamp=${TMPDIR:-/tmp}/mplangp-build-$(printf '%s' "$root" | cksum | cut -d' ' -f1)
	now=$(date +%s)
	pk=
	pt=0
	[ -f "$stamp" ] && read -r pk pt <"$stamp"
	[ -n "$k" ] && [ "$k" = "$pk" ] && [ $((now - ${pt:-0})) -le 2 ] && exit 0
fi
out=$(cd "$root" && make 2>&1)
st=$?
if [ $st -ne 0 ] || printf '%s' "$out" | grep -q 'warning:'; then
	[ -n "$stamp" ] && rm -f "$stamp"
	printf '%s\n' "$out" | grep -E 'error|warning|Error' | head -20 >&2
	exit 2
fi
[ -n "$stamp" ] && [ -n "$k" ] && echo "$k $now" >"$stamp"
