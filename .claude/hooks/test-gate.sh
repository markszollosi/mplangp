#!/bin/sh
# Stop: if code changed, make test must pass before the agent finishes.
in=$(cat)
[ "$(printf '%s' "$in" | jq -r '.stop_hook_active')" = true ] && exit 0
cd "${CLAUDE_PROJECT_DIR:-.}" || exit 0
[ -n "$(git status --porcelain -- src modules tests Makefile)" ] || exit 0
out=$(make test 2>&1)
[ $? -eq 0 ] && exit 0
jq -n --arg r "make test fails; fix before finishing:
$(printf '%s' "$out" | tail -25)" '{decision:"block",reason:$r}'
