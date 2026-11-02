# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

- `make` builds `./mplangp` and `modules/*.so`; `./mplangp file.my` runs a program.
- `make test` runs all module tests (`tests/test_<module>.c`, each with its own `main()`) and then `tests/run.sh`, which diffs every `tests/samples/*.my` against its `.expected`.
- One module test: `make tests/test_vm && ./tests/test_vm` (replace `vm` with value, lexer, ast, parser, chunk, table or compiler).
- Error cases (exit code + message) live in `tests/errors.sh`.
- `docs/` is the black-box documentation for programmers of the language (agents read it instead of the source). `tests/docs.sh` (part of `make test`) runs every ```` ```my ```` / ```` ```my-error ```` fenced example, compares a following ```` ```output ```` block, and fails when a function exported by `modules/*.c` or `src/builtin.c` is missing from its page. Changing language behaviour or a module means updating `docs/` in the same change.
- One sample: `./mplangp tests/samples/fizzbuzz.my`.
- Writing a native module: follow `modules/README.md` (required reading before touching `modules/` or `src/mplangpfunc.h`).
- Adding a source file to the interpreter: add it to `SRC` in the `Makefile` and, if it has a test, to `TESTS` plus a `T_<name>` source list.
- Leak check on macOS: `leaks --atExit -- ./mplangp file.my`.

## Layout

`src/` holds all sources and headers (including `main.c`), `modules/` the native module sources (built `.so` files are git-ignored), `tests/` the module tests and `tests/samples/`, `build/` the object files (git-ignored). The `Makefile` lives at the root and `SRC` lists names relative to `src/`.

## Conventions

Pure C99 built with `-std=c99 -Wall -Wextra -pedantic`, suckless/KISS style: tabs, function opening brace on its own line, snake_case, almost no comments, errors returned as bool/enum/NULL plus a `line N: message` on stderr. The language name is undecided; `mplangp` is the interpreter name, `.my` the source extension.

## Design goal and testing

The language is AI-native: optimized for agentic work, and Claude must be able to program in it. Favor agent ergonomics (clear `line N: message` errors, predictable semantics, accurate docs). Every new feature or functionality change must be tested against fresh agents in sandboxes with no knowledge of the implementation: give them only the user-facing docs and the `mplangp` binary (not `src/`), have them write programs in the language, and treat their failures (unclear errors, doc gaps, surprising semantics) as bugs. Passing unit tests is not enough.

## Architecture

Pipeline: `lexer -> parser -> AST -> compiler -> Chunk -> vm`, wired in `main.c`. Modules talk only through their headers.

Per-component invariants (scoping, ownership, opcodes, module ABI, known gaps) are not listed here: a hook injects the relevant `.claude/contracts/*.md` the first time you open a `src/` file, and a `SessionStart` hook prints the file map (`.claude/filemap.txt`; keep it current when adding files).

## Working economically

Do not survey the repo or read files "to get oriented". Use the file map, open only what the task needs, and grep for a specific symbol instead of reading whole files.
- Writing programs in the language: read `docs/README.md` only, never `src/`. To validate a change this way, use the `blackbox` agent.
- Module work: `modules/README.md`; the module/function index is injected on first touch, so do not read other modules to check for existing features.
- Interpreter work: the one component file, its `tests/test_<x>.c`, and the contract the hook injects.

## Issues

Context is scarce, so agents record bugs, doc gaps and follow-ups as GitHub issues instead of carrying them in memory or fixing them out of scope. Open one with `gh issue create --title "..." --body "..."` (title must be a quoted `--title`). The `issue-gate.sh` hook first lists similar open and closed issues and denies the call if it finds any: comment on the existing one (`gh issue comment N`), or if the issue is genuinely new rerun the command prefixed with `ISSUE_CHECKED=1`.
