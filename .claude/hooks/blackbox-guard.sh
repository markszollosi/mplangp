#!/bin/sh
# PreToolUse for the blackbox agent: no access to implementation sources.
in=$(cat)
root=$(cd "${CLAUDE_PROJECT_DIR:-.}" 2>/dev/null && pwd -P) || root=$(pwd -P)
cd "$root" || exit 0
tool=$(printf '%s' "$in" | jq -r '.tool_name // empty')
p=$(printf '%s' "$in" | jq -r '.tool_input.path // empty')
pat=$(printf '%s' "$in" | jq -r '.tool_input.pattern // empty')
t=$(printf '%s' "$in" | jq -r '[.tool_input.file_path, .tool_input.path, .tool_input.pattern, .tool_input.command] | map(select(. != null)) | join(" ")')
deny()
{
	jq -n --arg r "Black-box testing: the implementation is off limits ($1). Use docs/ and the mplangp binary only. A gap in docs/ is a finding: report it." \
		'{hookSpecificOutput:{hookEventName:"PreToolUse",permissionDecision:"deny",permissionDecisionReason:$r}}'
	exit 0
}
n=$(printf '%s' "$t" | tr -d "'\"\\\\" | sed "s|$root/||g")
bad='(^|[^A-Za-z0-9_.-])(src|tests|\.git|\.claude)([^A-Za-z0-9_.-]|$)|modules/[A-Za-z0-9_]+\.c|Makefile|CLAUDE\.md'
printf '%s' "$n" | grep -Eq "$bad" && deny "path"
printf '%s' "$n" | grep -Eq '(^|[ /])\.\.(/|[ ]|$)' && deny "parent directory"
printf '%s' "$n" | grep -Eq '\$\(|`|\$[A-Za-z_]|\$\{|\{[^}]*,[^}]*\}|(^|[^A-Za-z0-9_-])eval([^A-Za-z0-9_-]|$)' && deny "indirection"
printf '%s' "$n" | grep -Eq '(^|[^A-Za-z0-9_./-])git([^A-Za-z0-9_-]|$)' && deny "git"
case "$tool" in
Grep | Glob)
	case "$p" in
	docs | docs/*) ;;
	*) case "$tool:$pat" in Glob:docs/*) ;; *) deny "search outside docs/" ;; esac ;;
	esac ;;
esac
rec='(^|[^A-Za-z0-9_.-])(find|rg|tree|locate|mdfind|ag|ack|du)([^A-Za-z0-9_-]|$)|grep[ ]+-[A-Za-z]*[rR]|ls[ ]+-[A-Za-z]*R|--recursive'
set -f
if printf '%s' "$n" | grep -Eq "$rec"; then
	printf '%s' "$n" | grep -Eq '(^| )docs(/| |$)' || deny "recursive search"
	for w in $n; do
		case "$w" in . | ./ | / | '~' | '~/' | '*' | './*') deny "recursive search" ;; esac
	done
fi
exp=
for w in $n; do
	case "$w" in
	-*) continue ;;
	*[*?[]*) exp="$exp
$(set +f; for g in $w; do [ -e "$g" ] && printf '%s\n' "$g"; done)" ;;
	*) [ -e "$w" ] || continue
		if [ -d "$w" ]; then r=$(cd "$w" && pwd -P)
		else r=$(cd "$(dirname "$w")" && pwd -P)/$(basename "$w"); fi
		[ "$r" = "$root" ] || exp="$exp
${r#"$root"/}" ;;
	esac
done
printf '%s\n' "$exp" | grep -Eq "$bad" && deny "glob or symlink"
exit 0
