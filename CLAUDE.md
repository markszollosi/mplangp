# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

- `make` builds `./mplangp` and `modules/*.so`; `./mplangp file.my` runs a program.
- `make test` runs all module tests (`tests/test_<module>.c`, each with its own `main()`) and then `tests/run.sh`, which diffs every `tests/samples/*.my` against its `.expected`.
- One module test: `make tests/test_vm && ./tests/test_vm` (replace `vm` with value, lexer, ast, parser, chunk, table or compiler).
- Error cases (exit code + message) live in `tests/errors.sh`.
- One sample: `./mplangp tests/samples/fizzbuzz.my`.
- Adding a module: add it to `SRC` in the `Makefile` and, if it has a test, to `TESTS` plus a `T_<name>` source list.
- Leak check on macOS: `leaks --atExit -- ./mplangp file.my`.

## Layout

`src/` holds all sources and headers (including `main.c`), `modules/` the native module sources (built `.so` files are git-ignored), `tests/` the module tests and `tests/samples/`, `build/` the object files (git-ignored). The `Makefile` lives at the root and `SRC` lists names relative to `src/`.

## Conventions

Pure C99 built with `-std=c99 -Wall -Wextra -pedantic`, suckless/KISS style: tabs, function opening brace on its own line, snake_case, almost no comments, errors returned as bool/enum/NULL plus a `line N: message` on stderr. The language name is undecided; `mplangp` is the interpreter name, `.my` the source extension.

## Architecture

Pipeline: `lexer -> parser -> AST -> compiler -> Chunk -> vm`, wired in `main.c`. Modules talk only through their headers.

Contracts that span several files:
- The lexer emits INDENT/DEDENT/NEWLINE; blocks are `:` NEWLINE INDENT stmts DEDENT. Tokens and AST names point into the source buffer, so `main.c` must keep the source alive until `compile()` returns.
- `elif` is a nested `STMT_IF` in the parent's `else_branch`. Blocks are linked lists via `Stmt.next`.
- Scoping: at top level every variable is a global (`OP_SET_GLOBAL` / `OP_GET_GLOBAL`, name in the constant pool; `OP_DEFINE_GLOBAL` is unused). Inside a `def`, the compiler pre-scans the body: parameters plus every assigned name become locals (`OP_GET_LOCAL`/`OP_SET_LOCAL`, slot 0 is the callee, slots start at 1); other names stay global. Unassigned locals read as `None`. `def` is top-level only.
- Loops: `break` and `continue` need no opcodes. `continue` is an `OP_LOOP` back to the loop's condition; `break` is a forward `OP_JUMP` recorded in `loops.breaks` and patched after the loop's final `OP_POP`. Statement boundaries keep the stack clean, so no unwinding is needed. `loops.floor` stops a `break` inside a `def` from targeting a loop outside it.
- Functions: `compile` turns each `def` into a `Function` (own `Chunk`, `arity`, `nlocals`) stored as a `VAL_FUNC` constant and bound with `OP_SET_GLOBAL`. `Function` is refcounted (`value_copy` retains, `value_free` releases). The VM runs `Frame`s (fn, ip, stack base); `OP_CALL argc` pushes a frame and pads locals with `None`, `OP_RETURN` unwinds to the frame base. The main program runs as a pseudo-function in frame 0.
- Native modules: `import x` compiles to `OP_IMPORT` (pushes a `VAL_MODULE`) and `x.f` to `OP_GET_ATTR` (yields a borrowed `VAL_NATIVE` pointing at the module's `MpEntry`). `module.c` `dlopen`s `<path>/x.so` and reads the `mplangp_abi` / `mplangp_entries` symbols that the `MP_MODULE` macro in `src/mplangpfunc.h` defines. That header carries the `Value` layout plus inline `mp_*` helpers so modules never link against the interpreter; changing `Value` or bumping `MP_ABI` breaks existing `.so` files. `vm_free` frees globals before `dlclose`.
- `and`/`or` compile to jumps; there are no AND/OR opcodes. `OP_JUMP_IF_FALSE` does not pop, so the compiler emits explicit `OP_POP`s. Jump operands are 2-byte big-endian; constant operands are 1 byte, so a chunk holds at most 256 distinct constants (the compiler reuses equal ones).
- Ownership: a `Chunk` owns its constants, and `value_array_free` frees string elements. The VM pushes copies of constants. `table_set` takes ownership of the value; `table_get` returns a borrowed view.
- `program_free` frees the whole AST including the `Program` struct.

Known gaps: some parser error paths still leak half-built expressions; string literals with embedded NULs are truncated in the compiler.
