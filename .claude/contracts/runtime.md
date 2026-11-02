Contracts for the runtime (src/vm.*, value.*, function.*, table.*, builtin.*, util.*):
- `Function` is refcounted (`value_copy` retains, `value_free` releases). The VM runs `Frame`s (fn, ip, stack base); `OP_CALL argc` pushes a frame and pads locals with `None`, `OP_RETURN` unwinds to the frame base. The main program runs as a pseudo-function in frame 0.
- Ownership: the VM pushes copies of constants. `table_set` takes ownership of the value; `table_get` returns a borrowed view.
- Collections: `VAL_LIST` / `VAL_MAP` are refcounted (`List`, `Map` in `value.h`, shared by reference, maps are insertion-ordered with string keys and linear lookup). `value_copy` retains, `value_free` releases and recurses; cycles leak. The helpers (`mp_list`, `mp_push`, `mp_map`, `mp_set`, `mp_get`) live in `mplangpfunc.h` so modules and the interpreter share one implementation; `value_map_put` is the interpreter's variant that frees a replaced value (which may be a function).
- Builtins (`src/builtin.c`) are natives bound as globals by `vm_init`; they may use interpreter functions, unlike modules. `args()` returns the arguments after the script path. Names must not clash with module names (`str`, `math`), which is why the conversion builtin is `text`.
- `vm_free` frees globals before `dlclose`.
- Changing `Value` breaks built `.so` files (see native.md).
- Tests: tests/test_vm.c, test_value.c, test_table.c. A new builtin needs a page entry in docs/builtins.md (`tests/docs.sh` enforces it).
