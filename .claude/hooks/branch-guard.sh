#!/bin/sh
# PreToolUse(Bash|Edit|Write): on main, deny git commit and edits to
# src/, modules/, docs/; the reason carries the branch command to run.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
[ "$(git symbolic-ref --short -q HEAD)" = main ] || exit 0
ev=$(cat)
deny()
{
	jq -n --arg r "branch-guard: on main, $1. Branch per task first: git switch -c <name> (e.g. git switch -c issue-N or git switch -c feat-short-name)." \
		'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
	exit 0
}
cmd=$(printf '%s' "$ev" | jq -r '.tool_input.command // empty')
if [ -n "$cmd" ]; then
	printf '%s\n' "$cmd" | sed -E 's/(;|&&|\|\||\|)/\
/g' | grep -Eq '^[[:space:]]*([A-Za-z_][A-Za-z0-9_]*=[^[:space:]]*[[:space:]]+)*git([[:space:]]+(-C|-c)[[:space:]]+[^[:space:]]+|[[:space:]]+-[^[:space:]]+)*[[:space:]]+commit([[:space:]]|$)' &&
		deny "git commit is not allowed"
	exit 0
fi
f=$(printf '%s' "$ev" | jq -r '.tool_input.file_path // empty')
[ -n "$f" ] || exit 0
root=$(pwd -P)
f=${f#"$root"/}
case $f in
src/*|modules/*|docs/*) deny "editing $f is not allowed" ;;
esac
exit 0
