#!/bin/sh
# SessionStart: one line each for branch, open PRs, open issues. Silent
# when gh is missing, offline or unauthenticated.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
command -v gh >/dev/null 2>&1 || exit 0
t()
{
	if command -v timeout >/dev/null 2>&1; then timeout 8 "$@"
	elif command -v perl >/dev/null 2>&1; then perl -e 'alarm 8; exec @ARGV' "$@"
	else "$@"; fi
}
prs=$(t gh pr list --state open --limit 10 --json number,title,headRefName 2>/dev/null) || exit 0
iss=$(t gh issue list --state open --limit 15 --json number,title 2>/dev/null) || exit 0
msg="Branch: $(git branch --show-current 2>/dev/null)
Open PRs: $(printf '%s' "$prs" | jq -r 'map("#\(.number) \(.headRefName)") | join("; ") | if .=="" then "none" else . end')
Open issues: $(printf '%s' "$iss" | jq -r 'map("#\(.number) \(.title[:60])") | join("; ") | if .=="" then "none" else . end')"
jq -n --arg m "$msg" '{hookSpecificOutput:{hookEventName:"SessionStart",additionalContext:$m}}'
