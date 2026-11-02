#!/bin/sh
# PreToolUse: first touch of modules/ or mplangpfunc.h in a session injects
# the module index (names + functions) so agents need not read every module.
in=$(cat)
f=$(printf '%s' "$in" | jq -r '.tool_input.file_path // .tool_input.path // empty')
case "$f" in
*/modules/README.md) exit 0 ;;
*/modules/*.c | */modules/ | */modules | */src/mplangpfunc.h) ;;
*) exit 0 ;;
esac
sid=$(printf '%s' "$in" | jq -r '.session_id')
mark="${TMPDIR:-/tmp}/mplangp-modctx-$sid"
[ -e "$mark" ] && exit 0
: > "$mark"
root=${f%/modules*}
[ "$root" = "$f" ] && root=${f%/src/*}
msg="Native module work. Read modules/README.md first (rules, signature, ownership). Do NOT read other modules to check for existing features; this index is authoritative (reference examples: math.c, str.c). Existing modules and functions:"
for m in "$root"/modules/*.c; do
	[ -f "$m" ] || continue
	fns=$(awk '/MP_MODULE/{p=1} p' "$m" | grep -o '{"[A-Za-z0-9_]*"' | tr -d '{"' | tr '\n' ' ')
	msg="$msg
- $(basename "$m" .c): $fns"
done
case "$f" in
*mplangpfunc.h) msg="$msg
Changing Value layout or bumping MP_ABI breaks every built .so: rebuild all modules and update modules/README.md." ;;
esac
jq -n --arg m "$msg" '{hookSpecificOutput:{hookEventName:"PreToolUse",additionalContext:$m}}'
