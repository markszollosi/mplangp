#!/bin/sh
# Stop: src/modules changes must come with docs, error-test cases and module samples.
in=$(cat)
[ "$(printf '%s' "$in" | jq -r '.stop_hook_active')" = true ] && exit 0
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
base=$(git merge-base HEAD main 2>/dev/null || git merge-base HEAD origin/main 2>/dev/null || echo HEAD)
new=$(git ls-files -o --exclude-standard -- src modules docs)
names=$( { git diff --name-only "$base" -- src modules docs; printf '%s\n' "$new"; } | sed '/^$/d')
printf '%s\n' "$names" | grep -qE '^(src|modules)/.*\.[ch]$' || exit 0
miss=
printf '%s\n' "$names" | grep -q '^docs/' || miss="$miss
docs/: no change for src/modules edits"
added=$( { git diff -U0 "$base" -- 'src/*.c' 'modules/*.c' | grep '^+[^+]'
	for f in $(printf '%s\n' "$new" | grep -E '^(src|modules)/.*\.c$'); do cat "$f"; done; } 2>/dev/null)
msgs=$(printf '%s\n' "$added" | grep -E '(error_at|fail|error|errf|mp_error)\(' |
	sed -n 's/^[^"]*"\([^"%]\{4,\}\).*/\1/p' | sort -u)
IFS='
'
for m in $msgs; do
	grep -qF -- "$m" tests/errors.sh || miss="$miss
tests/errors.sh: no case for \"$m\""
done
for f in $(git diff --name-only --diff-filter=A "$base" -- 'modules/*.c'; printf '%s\n' "$new" | grep '^modules/.*\.c$'); do
	n=$(basename "$f" .c)
	grep -qE "^[[:space:]]*(import|from)[[:space:]]+$n\b" tests/samples/*.my 2>/dev/null ||
		miss="$miss
tests/samples/: no sample imports module $n"
done
[ -n "$miss" ] || exit 0
jq -n --arg r "sync missing:$miss" '{decision:"block",reason:$r}'
