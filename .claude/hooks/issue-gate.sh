#!/bin/sh
# PreToolUse(Bash): before `gh issue create`, look for similar issues
# (open and closed). Deny with the candidates; retry with ISSUE_CHECKED=1
# in front of the command once it is confirmed to be new.
cmd=$(jq -r '.tool_input.command // empty')
case "$cmd" in
*"gh issue create"*) ;;
*) exit 0 ;;
esac
case "$cmd" in
*ISSUE_CHECKED=1*) exit 0 ;;
esac
deny()
{
	jq -n --arg r "$1" \
		'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
	exit 0
}
title=$(printf '%s' "$cmd" | sed -En \
	-e "s/.*(--title|-t)[ =]*\"([^\"]*)\".*/\2/p" \
	-e "s/.*(--title|-t)[ =]*'([^']*)'.*/\2/p" | head -1)
[ -n "$title" ] || deny "issue-gate: could not read the title; pass --title \"...\" with a quoted string so existing issues can be checked."
all=$(gh issue list --state all --limit 300 --json number,title,state,body 2>&1) ||
	deny "issue-gate: could not list issues ($all). Fix gh auth/network, or retry with ISSUE_CHECKED=1 after checking by hand."
hits=$(printf '%s' "$all" | jq -r --arg t "$title" '
	def words: ascii_downcase | [scan("[a-z0-9_.]{4,}")] | unique;
	($t | words) as $k
	| select(($k | length) > 0)
	| .[]
	| (.title + " " + (.body // "")) as $h
	| select(([$k[] | select(. as $w | $h | ascii_downcase | contains($w))] | length) as $n
		| $n >= 2 or ($n >= 1 and ($k | length) <= 2))
	| "#\(.number) [\(.state)] \(.title)"' | head -8)
[ -n "$hits" ] || exit 0
deny "issue-gate: possibly existing issues for \"$title\":
$hits
Inspect with gh issue view N. If one covers it, comment there (gh issue comment N) instead. If it is genuinely new, rerun the same command prefixed with ISSUE_CHECKED=1."
