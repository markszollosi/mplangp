# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

- `make` builds `./mplangp`; `./mplangp file.my` runs a program.
- `make test` runs all module tests (`tests/test_<module>.c`, each with its own `main()`) and then `tests/run.sh`, which diffs every `tests/samples/*.my` against its `.expected`.
- One module test: `make tests/test_vm && ./tests/test_vm` (replace `vm` with value, lexer, ast, parser, chunk, table or compiler).
- One sample: `./mplangp tests/samples/fizzbuzz.my`.
- Adding a module: add it to `SRC` in the `Makefile` and, if it has a test, to `TESTS` plus a `T_<name>` source list.
- Leak check on macOS: `leaks --atExit -- ./mplangp file.my`.

## Layout

`src/` holds all sources and headers (including `main.c`), `tests/` the module tests and `tests/samples/`, `build/` the object files (git-ignored). The `Makefile` lives at the root and `SRC` lists names relative to `src/`.

## Conventions

Pure C99 built with `-std=c99 -Wall -Wextra -pedantic`, suckless/KISS style: tabs, function opening brace on its own line, snake_case, almost no comments, errors returned as bool/enum/NULL plus a `line N: message` on stderr. The language name is undecided; `mplangp` is the interpreter name, `.my` the source extension.

## Architecture

Pipeline: `lexer -> parser -> AST -> compiler -> Chunk -> vm`, wired in `main.c`. Modules talk only through their headers.

Contracts that span several files:
- The lexer emits INDENT/DEDENT/NEWLINE; blocks are `:` NEWLINE INDENT stmts DEDENT. Tokens and AST names point into the source buffer, so `main.c` must keep the source alive until `compile()` returns.
- `elif` is a nested `STMT_IF` in the parent's `else_branch`. Blocks are linked lists via `Stmt.next`.
- All variables are globals. The compiler emits only `OP_SET_GLOBAL` for assignment (`OP_DEFINE_GLOBAL` exists but is unused; the VM treats them the same) and `OP_GET_GLOBAL` for reads, with the name stored as a string constant.
- `and`/`or` compile to jumps; there are no AND/OR opcodes. `OP_JUMP_IF_FALSE` does not pop, so the compiler emits explicit `OP_POP`s. Jump operands are 2-byte big-endian; constant operands are 1 byte, so a chunk holds at most 256 distinct constants (the compiler reuses equal ones).
- Ownership: a `Chunk` owns its constants, and `value_array_free` frees string elements. The VM pushes copies of constants. `table_set` takes ownership of the value; `table_get` returns a borrowed view.
- `program_free` frees the whole AST including the `Program` struct.

Known gaps: the parser leaks half-built expressions on a syntax error; string literals with embedded NULs are truncated in the compiler; no user-defined functions yet.
