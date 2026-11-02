#!/bin/sh
# SessionStart: print the file map, flag src files missing from it.
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
msg="Repo map (do not survey the repo; open only what the task needs):
$(cat .claude/filemap.txt)"
for f in src/*.c; do
	b=$(basename "$f" .c)
	grep -q "src/$b[.\[ ]" .claude/filemap.txt || msg="$msg
UNMAPPED: $f (add it to .claude/filemap.txt)"
done
jq -n --arg m "$msg" '{hookSpecificOutput:{hookEventName:"SessionStart",additionalContext:$m}}'
