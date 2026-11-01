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
`if` / `elif` / `else`, `while` with `break` and `continue`, and the builtin `print(expr)`.
Blocks are introduced by `:` and indentation (spaces only, tabs are an error).

Functions: `def name(a, b):` with `return` (a bare `return`, or falling off the end, gives
`None`). Parameters and any name assigned inside a function are local; other names read
globals. Functions can recurse, but `def` cannot be nested and there are no default arguments.

	i = 0
	while i < 5:
	    if i % 2 == 0:
	        print(i)
	    i = i + 1

## Native modules

`import name` loads `modules/name.so` and binds a module value; its functions are called as
`name.func(...)`. Modules are searched in `<script dir>/modules`, then each directory of
`$MPLANGP_MODULES` (colon separated), then `./modules`.

A module is one C file that includes `mplangpfunc.h` and needs nothing from the interpreter:

	#include "mplangpfunc.h"

	static bool m_double(int argc, Value *argv, Value *ret)
	{
		if (argc != 1 || !mp_is_num(argv[0]))
			return mp_error(ret, "double(x) takes one number.");
		*ret = mp_float(mp_to_float(argv[0]) * 2);
		return true;
	}

	MP_MODULE({"double", m_double});

Put it in `modules/`; `make modules` builds every `modules/*.c` to a `.so`. Arguments are
borrowed, the result goes in `*ret`, and `return mp_error(ret, "msg")` raises a runtime error.
`modules/math.c` and `modules/str.c` are working examples; `modules/README.md` is the full guide.

## Layout

	src/      interpreter sources and headers (mplangpfunc.h is the module API)
	modules/  native modules: .c sources, built .so files
	tests/    module tests and sample programs
	build/    object files (generated)

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
| `function` | compiled function object (refcounted) |
| `module`, `mplangpfunc.h` | `dlopen` loader and the API native modules use |
| `vm` | stack-based evaluator with call frames |
| `main.c` | wires the pipeline together |

## License

Copyright (c) 2026 Mark Szollosi. All rights reserved. See `LICENSE`.
