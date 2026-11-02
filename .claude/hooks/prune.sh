#!/bin/sh
# SessionStart: drop stale agent worktrees and old session markers.
cd "${CLAUDE_PROJECT_DIR:-.}" 2>/dev/null || exit 0
top=$(git rev-parse --show-toplevel 2>/dev/null) || exit 0
git worktree prune 2>/dev/null
cur=$(pwd -P)
for d in "$top"/.claude/worktrees/*/; do
	[ -d "$d" ] || continue
	d=${d%/}
	[ "$(cd "$d" && pwd -P)" = "$cur" ] && continue
	if ! git worktree list --porcelain | grep -Fqx "worktree $d"; then
		[ -e "$d/.git" ] && continue
		[ -n "$(find "$d" -prune -mtime +7 2>/dev/null)" ] && rm -rf "$d"
		continue
	fi
	[ -n "$(find "$d" -prune -mtime +7 2>/dev/null)" ] || continue
	[ -z "$(git -C "$d" status --porcelain 2>/dev/null)" ] || continue
	br=$(git -C "$d" symbolic-ref -q --short HEAD) || continue
	git merge-base --is-ancestor "$br" main 2>/dev/null || continue
	git worktree remove "$d" 2>/dev/null && git branch -d "$br" >/dev/null 2>&1
done
find "${TMPDIR:-/tmp}" -maxdepth 1 -name 'mplangp-*' -mtime +0 -exec rm -rf {} + 2>/dev/null
exit 0
