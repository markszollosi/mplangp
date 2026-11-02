#!/bin/sh
# PreToolUse: first touch of a src/ component injects only its contracts.
in=$(cat)
f=$(printf '%s' "$in" | jq -r '.tool_input.file_path // empty')
case "$f" in
*/src/lexer.* | */src/token.h | */src/parser.* | */src/ast.*) c=syntax ;;
*/src/compiler.* | */src/opcode.h | */src/chunk.*) c=compiler ;;
*/src/vm.* | */src/value.* | */src/function.* | */src/table.* | */src/builtin.* | */src/util.*) c=runtime ;;
*/src/module.* | */src/mplangpfunc.h) c=native ;;
*) exit 0 ;;
esac
sid=$(printf '%s' "$in" | jq -r '.session_id')
mark="${TMPDIR:-/tmp}/mplangp-contract-$sid-$c"
[ -e "$mark" ] && exit 0
: > "$mark"
root=${f%/src/*}
jq -n --rawfile m "$root/.claude/contracts/$c.md" \
	'{hookSpecificOutput:{hookEventName:"PreToolUse",additionalContext:$m}}'
