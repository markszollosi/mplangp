#!/bin/sh
# PreToolUse for the docsync agent: Edit/Write only inside docs/.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in
*/docs/* | docs/*) exit 0 ;;
esac
jq -n --arg r "docsync may only edit docs/; $f is outside it." \
	'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
