# mplangp

A minimal, modular interpreter in pure C99, following the
[suckless](https://suckless.org/coding_style/) and KISS principles.
The language is Python-flavoured (indentation blocks, dynamic typing) but is
its own thing and has no name yet. Source files use the `.my` extension.

## Build and run

	make
	./mplangp program.my

`make test` builds and runs every module's standalone test, then runs each
`tests/samples/*.my` program and diffs its output against the `.expected` file.

Exit codes: 0 ok, 65 syntax or compile error, 70 runtime error, 1 usage or I/O.

## Language

Dynamic variables (`int`, `float`, `bool`, `str`, `None`), arithmetic
(`+ - * / %`, `/` always yields a float), comparisons, `and` / `or` / `not`,
`if` / `elif` / `else`, `while`, and the builtin `print(expr)`.
Blocks are introduced by `:` and indentation (spaces only, tabs are an error). There are no
user-defined functions yet.

	i = 0
	while i < 5:
	    if i % 2 == 0:
	        print(i)
	    i = i + 1

## Architecture

Each module is one cogwheel with one job, connected through a small header:

	source -> lexer -> parser -> AST -> compiler -> bytecode chunk -> vm

| module | job |
|---|---|
| `util` | checked allocation, `die` |
| `value` | dynamic value type and arrays of values |
| `lexer`, `token.h` | source text to tokens, including indent/dedent |
| `ast` | expression and statement nodes |
| `parser` | tokens to AST (recursive descent) |
| `opcode.h`, `chunk` | bytecode format and byte/constant buffer |
| `compiler` | AST to bytecode |
| `table` | string-keyed hash table, used for globals |
| `vm` | stack-based bytecode evaluator |
| `main.c` | wires the pipeline together |
