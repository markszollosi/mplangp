#!/bin/sh
# PreToolUse for the blackbox agent: no access to implementation sources.
in=$(cat)
t=$(printf '%s' "$in" | jq -r '[.tool_input.file_path, .tool_input.path, .tool_input.pattern, .tool_input.command] | map(select(. != null)) | join(" ")')
if printf '%s' "$t" | grep -Eq '(^|[ /])(src|tests|\.git|\.claude)(/|[ ]|$)|modules/[A-Za-z0-9_]+\.c|Makefile|CLAUDE\.md'; then
	jq -n '{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:"Black-box testing: the implementation is off limits. Use docs/ and the mplangp binary only. A gap in docs/ is a finding: report it."}}'
fi
