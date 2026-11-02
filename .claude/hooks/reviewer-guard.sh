#!/bin/sh
# PreToolUse for the reviewer agent: Bash only for read-only git inspection.
c=$(jq -r '.tool_input.command // empty')
if printf '%s' "$c" | grep -Eq '^git (diff|log|show|status)( |$)' && ! printf '%s' "$c" | grep -Eq '[;&|><`$]'; then
	exit 0
fi
jq -n '{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:"reviewer may only run a single git diff/log/show/status."}}'
