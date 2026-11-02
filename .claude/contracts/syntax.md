Contracts for lexer / parser / AST (src/lexer.*, token.h, parser.*, ast.*):
- The lexer emits INDENT/DEDENT/NEWLINE; blocks are `:` NEWLINE INDENT stmts DEDENT. Tokens and AST names point into the source buffer, so `main.c` must keep the source alive until `compile()` returns.
- `elif` is a nested `STMT_IF` in the parent's `else_branch`. Blocks are linked lists via `Stmt.next`.
- `program_free` frees the whole AST including the `Program` struct.
- Known gaps: some parser error paths still leak half-built expressions; `\x00` is rejected in string literals.
- Tests: tests/test_lexer.c, test_parser.c, test_ast.c. Language behaviour changes also need docs/ (docs/language.md).
