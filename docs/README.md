# mplangp documentation

**Start here if you are an agent or a person who must write programs in this language without reading the interpreter.**
Everything observable about the language is documented, and every example is run by `make test`, so
the code you copy works.

| document | contents |
|---|---|
| [language.md](language.md) | complete language reference: syntax, types, operators, scope, errors, limits |
| [builtins.md](builtins.md) | `len push pop keys values has get remove type text int float range sort slice args` |
| [modules/README.md](modules/README.md) | index of the 20 modules, then one page each |
| [cookbook.md](cookbook.md) | complete programs and defensive patterns |

## Run a program

```
./mplangp program.my [args...]      # from the repository root (modules are found in ./modules)
```
Exit codes: `0` ok, `65` syntax error (nothing ran), `70` runtime error, `1` usage / unreadable file, or the status given to `os.exit`. Errors print as `line N: message` on stderr.
The modules must be built first (`make`). If you run the binary from another directory, set `MPLANGP_MODULES=/path/to/modules`.

## The language in one screen

```my
import str
import json

def describe(person):
    label = person["name"]
    if person["age"] >= 18 and not ("minor" in person):
        label = label + " (adult)"
    return label

people = [
    {"name": "ada", "age": 36},
    {"name": "bob", "age": 12, "minor": True},
]
names = []
for p in people:
    push(names, describe(p))

counts = {}
for ch in "hello":
    if has(counts, ch):
        counts[ch] = counts[ch] + 1
    else:
        counts[ch] = 1

print(str.join(names, "; "))
print(counts)
print(json.dump(people[0]))
i = 0
while i < 3:
    i = i + 1
print(i * 2.5)
```

```output
ada (adult); bob
{"h": 1, "e": 1, "l": 2, "o": 1}
{"name":"ada","age":36}
7.5
```

## Rules that cause most mistakes

Read these before writing code.

1. **No exceptions.** Any error ends the program. Check before acting: `fs.exists(p)`, `has(m, k)`, `len(xs) > 0`, `proc.which(cmd) != None`. External commands are the exception: `proc.run` returns failures as data.
2. **A function cannot reassign a global.** Assigning a name inside `def` makes it local to that whole function. Mutate a shared list/map instead (`state["n"] = state["n"] + 1`) or return the new value.
3. **`print` takes exactly one value.** `print(a, b)` is a syntax error. Use `print(a + " " + text(b))`. `+` does not mix types: `"n=" + 5` is an error; write `"n=" + text(5)`.
4. **No `+=`, `//`, `**`, `not in`, `pass`, ternary, slicing syntax, tuples, `try`, `class`, f-strings, default arguments.** Use `x = x + 1`, `int(a / b)`, `math.pow`, `not (x in c)`, a lone `None` statement, `if/else`, `slice(x, a, b)`, maps/lists.
5. **`/` always gives a float.** `10 / 5` is `2.0`. Use `int(a / b)` for whole-number division. `%` follows Python (never negative for a positive divisor).
6. **Comparisons do not chain.** `a < b < c` is wrong; write `a < b and b < c`.
7. **Lists and maps are references.** `b = a` shares one list. Copy with `slice(a, 0, len(a))` or a loop.
8. **Map keys are strings.** `m[1]` is an error; use `m[text(1)]`. A missing key is an error: use `has` or `get(m, k, default)`.
9. **Float literals need digits both sides and no exponent**: `0.5` yes, `.5`, `5.` and `1e3` no (`float("1e3")` works).
10. **Strings are bytes, quotes are `"` or `'`, and only `\n \r \t \e \" \\ \xHH` are escapes.** `\'` is not an escape. Strings never contain NUL.
11. **Define before use.** A `def` runs when execution reaches it; call functions only after. `def` is allowed only at top level, never inside another function.
12. **`for` loops** iterate lists, map keys (in insertion order) and string bytes; use `range(n)` for counting. Blocks need at least one statement (`None` is a no-op).
13. **Regex is POSIX**: `[[:digit:]]`, not `\d`.
14. **Parameter and variable names hide builtins and modules** inside that function: `def f(text)` kills `text(...)`, `get`, `keys`, `type`, `len`, `str`, `time`... Pick other names (`text_in`, `key_list`, `kind`).
15. **Recursion is limited to 9999 levels** (fewer if each frame uses many locals; the value stack has 65536 slots). Runaway recursion stops with `line N: Stack overflow: ...`. Prefer loops for deep iteration.
16. **Numbers to text**: `text(x)` for plain conversion, `str.fixed(x, 2)` for fixed decimals, `str.pad_left/pad_right` for aligned columns.
17. **stdout is buffered**; use `io.flush()` or `io.eprint` when interleaving with stderr or other processes.

## How to be productive

- Look up the function you need in [modules/README.md](modules/README.md) ("Choosing a module"). Do not assume a Python function exists; confirm it in the module page.
- Anything missing from the language is usually available by running a program: `proc.run(["sort", "-u", file])`, `proc.sh("du -sh * | sort -h")`.
- Test a snippet quickly: write it to a file and run it; the error message names the line. Runtime errors point to the line that failed.
- Programs are plain text files. There is no REPL.
