#!/bin/sh
# SessionStart: print the file map, flag source/module/test files missing from it.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
msg="Repo map (do not survey the repo; open only what the task needs):
$(cat .claude/filemap.txt)"
un=$(sh tests/structure.sh map 2>&1 | sed -n 's/^structure: \(.*\) missing from .*/UNMAPPED: \1 (add it to .claude\/filemap.txt)/p')
[ -n "$un" ] && msg="$msg
$un"
jq -n --arg m "$msg" '{hookSpecificOutput:{hookEventName:"SessionStart",additionalContext:$m}}'
