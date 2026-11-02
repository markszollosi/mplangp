#!/bin/sh
# PreToolUse: refuse edits to build artifacts and the license.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in
*/tests/test_*.c) exit 0 ;;
*.so | */build/* | */LICENSE | */tests/test_*) ;;
*) exit 0 ;;
esac
jq -n --arg r "$f is a build artifact or protected file; edit the source and run make instead." \
	'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
