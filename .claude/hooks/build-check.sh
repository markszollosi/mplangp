#!/bin/sh
# PostToolUse: rebuild after a C edit; report only warnings/errors.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in
*/src/*.[ch]) root=${f%/src/*} ;;
*/modules/*.c) root=${f%/modules/*} ;;
*) exit 0 ;;
esac
out=$(cd "$root" && make 2>&1)
st=$?
if [ $st -ne 0 ] || printf '%s' "$out" | grep -q 'warning:'; then
	printf '%s\n' "$out" | grep -E 'error|warning|Error' | head -20 >&2
	exit 2
fi
