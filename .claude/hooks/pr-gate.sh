#!/bin/sh
# PreToolUse(Bash): local CI. Before a push or PR, build and test the
# committed HEAD in a clean clone (no untracked files, no stale objects).
cmd=$(jq -r '.tool_input.command // empty')
case "$cmd" in
*"git push"* | *"gh pr create"* | *"gh pr ready"*) ;;
*) exit 0 ;;
esac
top=$(git rev-parse --show-toplevel 2>/dev/null) || exit 0
tmp=$(mktemp -d "${TMPDIR:-/tmp}/mplangp-ci.XXXXXX") || exit 0
trap 'rm -rf "$tmp"' EXIT
git clone -q --local "$top" "$tmp/src" || exit 0
out=$(cd "$tmp/src" && make 2>&1 && make test 2>&1)
st=$?
if [ $st -eq 0 ] && printf '%s' "$out" | grep -q 'warning:'; then
	st=1
fi
[ $st -eq 0 ] && exit 0
reason="Local CI failed on committed HEAD; fix, commit, retry:
$(printf '%s' "$out" | grep -E '^FAIL|warning:|error:|\*\*\*' | head -20)"
jq -n --arg r "$reason" \
	'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
