#!/bin/sh
# Feeds fixture events to every .claude/hook and asserts allow/deny/context.
cd "$(dirname "$0")/.." || exit 1
root=$(pwd -P)
H=$root/.claude/hooks
T=$(mktemp -d "${TMPDIR:-/tmp}/hooks-test.XXXXXX") || exit 1
trap 'rm -rf "$T"' EXIT
export TMPDIR=$T CLAUDE_PROJECT_DIR=$root
fail=0
n=0

ok()
{
	n=$((n + 1))
	if [ "$1" = ok ]; then return; fi
	echo "FAIL: hooks: $2"
	fail=1
}

fx()
{
	sed "s|__ROOT__|${2:-$root}|g" "$root/tests/hooks/$1.json"
}

bash_ev()
{
	jq -nc --arg c "$1" '{tool_name:"Bash",session_id:"t1",tool_input:{command:$c}}'
}

has()
{
	case "$1" in *"$2"*) ok ok ;; *) ok no "$3: missing '$2' in: $1" ;; esac
}

hasnt()
{
	case "$1" in *"$2"*) ok no "$3: unexpected '$2'" ;; *) ok ok ;; esac
}

o=$(fx session | "$H/session-map.sh")
has "$o" '"additionalContext"' "session-map context"
has "$o" 'src/main.c' "session-map lists the map"
hasnt "$o" UNMAPPED "session-map all mapped"

o=$(fx read-module | "$H/module-context.sh")
has "$o" 'Existing modules and functions' "module-context first touch"
has "$o" '- math:' "module-context lists math"
o=$(fx read-module | "$H/module-context.sh")
[ -z "$o" ] && ok ok || ok no "module-context second touch must be silent"
o=$(printf '%s' '{"tool_name":"Read","session_id":"t1","tool_input":{"file_path":"'"$root"'/modules/README.md"}}' | "$H/module-context.sh")
[ -z "$o" ] && ok ok || ok no "module-context README must be silent"

o=$(fx read-src | "$H/contract.sh")
has "$o" '"additionalContext"' "contract first touch"
o=$(fx read-src | "$H/contract.sh")
[ -z "$o" ] && ok ok || ok no "contract second touch must be silent"
o=$(fx read-module | "$H/contract.sh")
[ -z "$o" ] && ok ok || ok no "contract ignores modules"

o=$(fx edit-build | "$H/protect.sh")
has "$o" '"deny"' "protect build/"
o=$(fx edit-src | "$H/protect.sh")
[ -z "$o" ] && ok ok || ok no "protect allows src"
o=$(fx edit-test-src | "$H/protect.sh")
[ -z "$o" ] && ok ok || ok no "protect allows tests/test_*.c"

o=$(bash_ev 'ls -l' | "$H/pr-gate.sh")
[ -z "$o" ] && ok ok || ok no "pr-gate ignores plain commands"
o=$(cd "$T" && bash_ev 'git push origin x' | "$H/pr-gate.sh")
[ -z "$o" ] && ok ok || ok no "pr-gate outside a repo allows"

o=$(bash_ev 'echo hi' | "$H/issue-gate.sh")
[ -z "$o" ] && ok ok || ok no "issue-gate ignores other commands"
o=$(bash_ev 'gh issue create --body x' | "$H/issue-gate.sh")
has "$o" '"deny"' "issue-gate needs a title"
o=$(fx issue-checked | "$H/issue-gate.sh")
[ -z "$o" ] && ok ok || ok no "issue-gate ISSUE_CHECKED bypass"

mkdir -p "$T/bc/src" "$T/bc/modules"
printf 'all:\n\t@echo "x.c:1: warning: boom"\n' > "$T/bc/Makefile"
fx edit-src "$T/bc" | "$H/build-check.sh" 2>/dev/null
[ $? -eq 2 ] && ok ok || ok no "build-check exit 2 on warning"
printf 'all:\n\t@true\n' > "$T/bc/Makefile"
fx edit-src "$T/bc" | "$H/build-check.sh" 2>/dev/null
[ $? -eq 0 ] && ok ok || ok no "build-check clean build exits 0"
fx read-src "$T/bc" | sed 's|src/vm.c|docs/a.md|' | "$H/build-check.sh" 2>/dev/null
[ $? -eq 0 ] && ok ok || ok no "build-check ignores non-C"

bb=$T/bb
mkdir -p "$bb/docs" "$bb/src" "$bb/tests" "$bb/modules"
: > "$bb/src/vm.c"
: > "$bb/docs/README.md"
: > "$bb/mplangp"
: > "$bb/prog.my"
ln -s ../src "$bb/docs/link"
ln -s src "$bb/lnk"
guard()
{
	CLAUDE_PROJECT_DIR=$bb "$H/blackbox-guard.sh"
}
deny()
{
	o=$(bash_ev "$1" | guard)
	has "$o" '"deny"' "blackbox denies: $1"
}
allow()
{
	o=$(bash_ev "$1" | guard)
	[ -z "$o" ] && ok ok || ok no "blackbox allows: $1 (got $o)"
}

deny 'cat src/vm.c'
deny 'cd src && cat vm.c'
deny 'cat s*c/vm.c'
deny 'cat s?c/vm.c'
deny 'cat [s]rc/vm.c'
deny 'cat {s,}rc/vm.c'
deny 'ls *'
deny 'cat $(echo src)/vm.c'
deny 'cat `echo src`/vm.c'
deny 'd=sr; cat ${d}c/vm.c'
deny "cat s''rc/vm.c"
deny 'cat lnk/vm.c'
deny 'cat docs/link/vm.c'
deny 'cat docs/../src/vm.c'
deny 'git -C . show HEAD:src/vm.c'
deny 'git show HEAD:vm.c'
deny 'eval "cat sr"c/vm.c'
deny 'find . -name "*.c"'
deny 'grep -r foo .'
deny 'cat Makefile'
deny 'cat modules/math.c'
deny 'cat CLAUDE.md'
allow './mplangp prog.my'
allow 'cat docs/README.md'
allow 'ls docs'
allow 'grep -rn print docs'
allow 'find docs -name "*.md"'
allow './mplangp prog.my | head -5'
allow 'ls *.my'
allow 'echo hi > out.txt'

o=$(jq -nc '{tool_name:"Read",tool_input:{file_path:"docs/README.md"}}' | guard)
[ -z "$o" ] && ok ok || ok no "blackbox Read docs"
o=$(jq -nc '{tool_name:"Read",tool_input:{file_path:"docs/link/vm.c"}}' | guard)
has "$o" '"deny"' "blackbox Read via symlink"
o=$(jq -nc '{tool_name:"Grep",tool_input:{pattern:"print"}}' | guard)
has "$o" '"deny"' "blackbox Grep without path"
o=$(jq -nc '{tool_name:"Grep",tool_input:{pattern:"print",path:"docs"}}' | guard)
[ -z "$o" ] && ok ok || ok no "blackbox Grep docs"
o=$(jq -nc '{tool_name:"Glob",tool_input:{pattern:"**/*.c"}}' | guard)
has "$o" '"deny"' "blackbox Glob everything"
o=$(jq -nc '{tool_name:"Glob",tool_input:{pattern:"docs/*.md"}}' | guard)
[ -z "$o" ] && ok ok || ok no "blackbox Glob docs"

mkdir -p "$T/pr/.claude/worktrees/stale"
(
	cd "$T/pr" && git init -q . && git -c user.email=a@b -c user.name=n -c commit.gpgsign=false commit -q --allow-empty -m i &&
		git branch -M main &&
		git worktree add -q .claude/worktrees/wt1 -b wt1 && git worktree add -q .claude/worktrees/wt2 -b wt2 &&
		: > .claude/worktrees/wt2/new && touch -t 200001010000 .claude/worktrees/wt1 .claude/worktrees/wt1/.git "$(git -C .claude/worktrees/wt1 rev-parse --absolute-git-dir)/HEAD" "$(git -C .claude/worktrees/wt1 rev-parse --absolute-git-dir)/index" .claude/worktrees/wt2 \
			.claude/worktrees/stale
) >/dev/null 2>&1
touch -t 200001010000 "$T/mplangp-old"
: > "$T/mplangp-old"
touch -t 200001010000 "$T/mplangp-old"
: > "$T/mplangp-new"
CLAUDE_PROJECT_DIR=$T/pr "$H/prune.sh"
[ -d "$T/pr/.claude/worktrees/wt1" ] && ok no "prune removes clean merged stale worktree" || ok ok
[ -d "$T/pr/.claude/worktrees/wt2" ] && ok ok || ok no "prune keeps dirty worktree"
[ -d "$T/pr/.claude/worktrees/stale" ] && ok no "prune removes untracked stale dir" || ok ok
[ -e "$T/mplangp-old" ] && ok no "prune removes old marker" || ok ok
[ -e "$T/mplangp-new" ] && ok ok || ok no "prune keeps fresh marker"

[ $fail -eq 0 ] && echo "PASS: hooks ($n checks)"
exit $fail
