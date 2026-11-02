Contracts for native module loading (src/module.*, mplangpfunc.h):
- `import x` compiles to `OP_IMPORT` (pushes a `VAL_MODULE`) and `x.f` to `OP_GET_ATTR` (yields a borrowed `VAL_NATIVE` pointing at the module's `MpEntry`). `module.c` `dlopen`s `<path>/x.so` and reads the `mplangp_abi` / `mplangp_entries` symbols that the `MP_MODULE` macro in `src/mplangpfunc.h` defines.
- That header carries the `Value` layout plus inline `mp_*` helpers so modules never link against the interpreter; changing `Value` or bumping `MP_ABI` breaks existing `.so` files.
- Modules needing a system library (`http` -> libcurl, `db` -> sqlite3) get it through `LIB_<name>` in the Makefile. Resources (sockets, databases) are integer handles. Strings cannot hold NUL, so module code uses `mp_string_clean` for external data.
- Writing a module: modules/README.md. A module change also needs its page in docs/modules/ (`tests/docs.sh` enforces it).
