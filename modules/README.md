# Writing a native module

Instructions for agents (and people) adding a module to mplangp. A module is
one C file in this folder that is built to a shared library and loaded by
`import name`. Read `math.c` and `str.c` next to this file first; they are the
reference examples.

## How it is used from mplangp code

    import math
    print(math.sqrt(16))      # 4.0

`import math` loads `modules/math.so` and binds the global `math`. `math.sqrt`
yields a native function that is called like any other. There is no
`from x import y`; members are always reached through the module name.

## Rules of the file

- The file name is the module name: `modules/foo.c` becomes `modules/foo.so`
  and is imported as `import foo`. Use only letters, digits and `_`.
- Include `mplangpfunc.h` and nothing from the interpreter. It carries the
  `Value` layout and every helper a module needs. Do not include `vm.h`,
  `table.h` or other `src/` headers and do not call interpreter functions
  (`value_string`, `xmalloc`, ...): a module does not link against the
  interpreter and such calls fail at load time.
- Export functions with exactly one `MP_MODULE(...)` at file scope, listing
  `{"mplangp_name", c_function}` pairs. The macro adds the terminator and the
  ABI symbols the loader checks. Do not write those by hand.
- Code style is the project's: C99, tabs, function brace on its own line,
  snake_case, almost no comments. It compiles with
  `-std=c99 -Wall -Wextra -pedantic`, so unused parameters must be handled
  (use them, or cast to `(void)`), and the build must stay warning free.

## The function signature

    static bool name(int argc, Value *argv, Value *ret);

- `argv[0..argc-1]` are the arguments. They are **borrowed**: never free them,
  never store the pointer, never keep a string pointer past the call.
- Check `argc` and every argument type yourself. The interpreter does not.
- Success: store the result in `*ret` and `return true`.
- Failure: `return mp_error(ret, "message");`. This makes the program stop with
  `line N: message` and exit code 70. Use a clear message that names the
  function and expected arguments, like `sqrt(x) takes one number.`
- `*ret` starts as `None`. A function with nothing to return can just
  `return true`.
- Return only `None`, bool, int, float, string, list or map (lists and maps
  holding those). Never return a function, native or module value.
- Strings you return must come from `mp_string` (it copies and mallocs; the
  interpreter frees it). Never return a pointer into `argv` or a static buffer.

## Helpers in `mplangpfunc.h`

| helper | meaning |
|---|---|
| `mp_none()`, `mp_bool(b)`, `mp_int(i)`, `mp_float(f)` | build a result `Value` |
| `mp_string(chars, len)` | build an owned string result (copy of `chars`) |
| `mp_is_int(v)` | int or bool |
| `mp_is_num(v)` | int, bool or float |
| `mp_to_int(v)`, `mp_to_float(v)` | read an int-like / numeric argument |
| `mp_error(ret, msg)` | set an error message, evaluates to `false` |
| `mp_errorf(ret, fmt, ...)` | same with printf formatting (240 chars max) |
| `MP_CHECK(cond, msg)` | `if (!(cond)) return mp_error(ret, msg);` |
| `mp_str(chars)` | owned string from a C string |
| `mp_string_clean(chars, len)` | owned string with NUL bytes dropped, for external data |
| `mp_is_str(v)` | string check |
| `mp_alloc(n)` | `malloc` that aborts on failure |
| `mp_list()`, `mp_push(list, item)` | build a list; `item` is owned by the list |
| `mp_map()`, `mp_set(map, key, val)` | build a map; `val` is owned, `key` is copied |
| `mp_get(map, key)`, `mp_find(map, key)` | look up a value (`NULL` / `-1` if absent) |
| `mp_value_release(v)` | free a value the module built itself and will not return |

Lists and maps are refcounted and read through their fields: `v.as.list->count`
and `->items[i]`, `v.as.map->count`, `->keys[i]`, `->vals[i]`. Arguments are
borrowed like everything else, so mutating an argument list in place (as `rand.shuffle` does)
is fine, but never store it. `modules/json.c` and `modules/db.c` are the best examples of
building nested results.

Type checks for strings: `argv[i].type == VAL_STRING`, then read
`argv[i].as.s` (NUL terminated). Other types are `VAL_NONE`, `VAL_BOOL`,
`VAL_INT`, `VAL_FLOAT`. If you need a helper that does not exist, add it to
`src/mplangpfunc.h` as `static inline` with an `mp_` prefix; it must not call
into the interpreter.

Semantics to match the language: bool counts as an int (`mp_is_num(True)` is
true), int and float mix, and a number divided or rooted into nothing useful
is an error, not `nan` or a crash.

## Minimal template

    #include "mplangpfunc.h"

    static bool m_double(int argc, Value *argv, Value *ret)
    {
    	if (argc != 1 || !mp_is_num(argv[0]))
    		return mp_error(ret, "double(x) takes one number.");
    	*ret = mp_float(mp_to_float(argv[0]) * 2);
    	return true;
    }

    MP_MODULE({"double", m_double});

## Build, test, done

1. `make modules` builds every `modules/*.c`. `make` and `make test` do too.
   Built `.so` files are git-ignored; commit only the `.c`.
2. Add a sample `tests/samples/<module>.my` (or extend `system.my`, `data.my`, `net_db.my`) that imports the module and prints
   results, then generate `tests/samples/<module>.expected` from the
   interpreter **after checking each line by hand or against Python**. Do not
   paste unverified output into an `.expected` file.
2b. A module that needs a library adds `LIB_<module> = -lfoo` to the `Makefile`
   (see `LIB_http`, `LIB_db`).
3. Add a `check 70 "message" 'program\n'` line to `tests/errors.sh` for each
   error your module can raise.
4. Run `make test`; it must exit 0. For a leak check on macOS:
   `leaks --atExit -- ./mplangp tests/samples/<module>.my` must report 0.
5. Write `docs/modules/<module>.md` (copy the shape of an existing page: purpose, function table with
   `` `name(args)` `` entries, errors, examples in ```` ```my ```` blocks followed by an ```` ```output ```` block)
   and add the module to `docs/modules/README.md`. `tests/docs.sh` fails if an exported function is
   missing from its page, and runs every example. Use ```` ```my-norun ```` for examples that need a
   network, a desktop or interactive input.

## Things that go wrong

- **`No module named 'x'`**: the `.so` is not where the loader looks. Order:
  `<script dir>/modules`, each entry of `$MPLANGP_MODULES` (colon separated),
  then `./modules`. Run from the repo root, or set `MPLANGP_MODULES`.
- **`not a compatible mplangp module`**: the `.so` has no `MP_MODULE`, or it
  was built against an older `Value` layout / `MP_ABI`. Rebuild everything.
- **Changing `Value` in `src/value.h` or the contract in `mplangpfunc.h`**
  breaks every existing module. Bump `MP_ABI` in `mplangpfunc.h` and rebuild
  all modules in the same change.
- **State:** file-scope `static` variables live as long as the module is
  loaded (until the interpreter exits), so they work for caches or counters.
  Do not hold interpreter values in them.
- **Not supported yet:** modules cannot call back into mplangp functions or
  define constants; only functions are exported. Resources such as sockets and
  databases are plain integers (a file descriptor or a slot number). Do not work around this with globals or hacks;
  extend `mplangpfunc.h` and the loader in `src/module.c` deliberately.
- The loader uses `dlopen`, which is POSIX rather than C99. The Linux build
  flags in the `Makefile` are untested.
