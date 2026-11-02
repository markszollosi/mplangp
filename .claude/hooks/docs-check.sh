#!/bin/sh
# PostToolUse: after a docs/*.md edit, run only tests/docs.sh; report failures.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in
*/docs/*.md) root=${f%/docs/*} ;;
*) exit 0 ;;
esac
[ -x "$root/mplangp" ] || (cd "$root" && make >/dev/null 2>&1)
out=$(cd "$root" && sh tests/docs.sh 2>&1)
st=$?
if [ $st -ne 0 ]; then
	printf '%s\n' "$out" | grep -v '^PASS' | head -20 >&2
	exit 2
fi
