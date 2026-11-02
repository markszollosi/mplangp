# Language reference

Complete description of the language as implemented. You do not need to read the
interpreter source: everything observable is here. Every `my` example in this file is executed by
`make test`, and the shown output is checked.

Run a program with `./mplangp program.my [args...]`. The file extension is `.my`.

## At a glance

- Python-flavoured: indentation blocks, dynamic types, `def`, `if/elif/else`, `while`, `for x in`.
- Types: `None`, `bool`, `int` (64-bit), `float`, `str` (bytes, no NUL), `list`, `map` (string keys), functions, modules.
- **No exceptions.** Any runtime error stops the program (exit code 70). Validate before you act.
- **Assignment inside a function never changes a global** (see [Scope](#scope)).
- The standard library is a set of native modules loaded with `import` (see [modules/README.md](modules/README.md)).

## Source text

- Encoding: bytes. UTF-8 text passes through unchanged but is treated as bytes (see [Strings](#strings)).
- Indentation: **spaces only**; a tab in indentation is a syntax error. Any deeper indentation opens a block; a dedent must return to a previous level. Use 4 spaces by convention.
- A statement ends at the end of the line. Inside `(...)`, `[...]` and `{...}` line breaks are ignored, so long literals and calls can span lines. There is no backslash line continuation.
- Comments start with `#` and run to the end of the line (not inside a string literal). Blank lines are ignored.
- A block (after `:`) must contain at least one statement. There is no `pass`; use a lone `None` as a no-op statement.

```my
total = 0   # a comment
values = [
    1,
    2,
    3,
]
for v in values:
    total = total + v
print(total)
if total > 100:
    None
else:
    print("small")
```

```output
6
small
```

## Types and literals

| type | literal examples | notes |
|---|---|---|
| `None` | `None` | the only "no value"; also what functions return by default |
| `bool` | `True`, `False` | behaves as integer 1/0 in arithmetic and comparisons |
| `int` | `0`, `42`, `-7` | signed 64-bit, **wraps on overflow** |
| `float` | `3.14`, `0.5` | needs digits on both sides of the dot; **no exponent literals** (`1e3` is a syntax error; use `float("1e3")`) |
| `str` | `"text"`, `'text'` | see [Strings](#strings) |
| `list` | `[1, "a", [2]]` | ordered, mutable, shared by reference |
| `map` | `{"k": 1}` | string keys, insertion ordered, mutable, shared by reference |
| function | `def f(): ...` | also native (builtin and module) functions |
| module | `import fs` | only supports `module.function` access |

`type(x)` returns `"none"`, `"bool"`, `"int"`, `"float"`, `"str"`, `"list"`, `"map"`, `"function"` (user and native functions alike) or `"module"`.

### Truthiness

False: `None`, `False`, `0`, `0.0`, `""`, `[]`, `{}`. Everything else is true (including functions and modules).

```my
print(not None)
print(not [])
print(not "0")
print([] or "default")
print(1 and "second")
```

```output
True
True
False
default
second
```

`and` / `or` short-circuit and return one of their operands (not necessarily a bool).

### Equality and ordering

- `==` / `!=` compare numbers by value across `int`, `float` and `bool` (`1 == 1.0 == True`).
- Values of different non-numeric types are never equal (`"1" == 1` is `False`).
- Lists and maps compare **structurally** (same elements; maps ignore order). Functions compare by identity.
- `<`, `<=`, `>`, `>=` work on two numbers or two strings (byte-wise). Anything else is a runtime error `Unsupported operand types.`
- **Comparisons do not chain like Python.** `1 < 2 < 3` means `(1 < 2) < 3`, i.e. `True < 3`. Write `1 < x and x < 3`.

```my
print(1 == 1.0)
print("1" == 1)
print([1, [2]] == [1, [2]])
print({"a": 1, "b": 2} == {"b": 2, "a": 1})
print("apple" < "banana")
```

```output
True
False
True
True
True
```

## Operators

Lowest to highest precedence:

| level | operators | associativity |
|---|---|---|
| 1 | `or` | left |
| 2 | `and` | left |
| 3 | `not` | prefix |
| 4 | `==` `!=` `<` `<=` `>` `>=` `in` | left |
| 5 | `+` `-` | left |
| 6 | `*` `/` `%` | left |
| 7 | unary `-` | prefix |
| 8 | call `f(x)`, index `x[i]`, attribute `m.name` | left |

Note `not` binds looser than comparisons (`not a == b` is `not (a == b)`) and unary minus binds tighter than `*`.

### Arithmetic

- `int op int` stays `int` for `+ - * %`; `/` **always returns a float** (`10 / 5` is `2.0`). There is no `//` and no `**`: use `int(a / b)` or `math.floor`, and `math.pow`.
- `%` follows the sign of the divisor like Python (`-7 % 3` is `2`). Division or modulo by zero is the runtime error `Division by zero.`
- Mixing `int` and `float` gives a `float`. `bool` counts as `0`/`1`.
- `str + str` concatenates; `str * int` repeats (a negative count gives `""`). `list + list` makes a new concatenated list. Anything else, such as `"a" + 1`, is `Unsupported operand types.`: convert with `text(x)`.

```my
print(7 / 2)
print(int(7 / 2))
print(-7 % 3)
print(2 * 3.0)
print("ab" * 3 + "!")
print([1] + [2, 3])
print("n=" + text(5))
print(9223372036854775807 + 1)
```

```output
3.5
3
2
6.0
ababab!
[1, 2, 3]
n=5
-9223372036854775808
```

### Membership: `in`

`x in container` returns a bool: for a list, whether an element equals `x`; for a map, whether `x` is a key (string); for a string, whether `x` is a substring. There is **no `not in`**; write `not (x in c)`.

```my
print(2 in [1, 2, 3])
print("k" in {"k": 0})
print("ell" in "hello")
print(not ("z" in "hello"))
c = "7"
print(c in "0123456789")
print("x" in "0123456789")
```

```output
True
True
True
True
True
False
```

`c in "0123456789"` is the idiom for a character-class test (digit, vowel, delimiter).

### Indexing

`x[i]` works on lists (element), strings (one-byte string) and maps (value for key). Negative list/string indexes count from the end. An out-of-range index is `Index out of range.`; a missing map key is `Key 'k' not found.` (use `has(m, k)` or `get(m, k, default)` first). There is **no slice syntax**; use `slice(x, from, to)`.

```my
xs = [10, 20, 30]
print(xs[0])
print(xs[-1])
print("abc"[1])
m = {"a": {"b": [1, 2]}}
print(m["a"]["b"][1])
```

```output
10
30
b
2
```

## Strings

- A string is a sequence of **bytes** and cannot contain a NUL byte. `len`, indexing, `slice` and `str.find` all count bytes, so non-ASCII UTF-8 text is passed through intact but `len("é")` is `2` and `"é"[0]` is half a character.
- Literals use `"` or `'`. They are single-line; there are no triple-quoted strings, no f-strings and no `%`/`format`. Build text with `+` and `text()`, or `str.join`.
- Escapes: `\n` `\r` `\t` `\e` (ESC, for ANSI codes) `\"` `\\` and `\xHH` (two hex digits, not `00`). **Any other backslash sequence is kept literally** (`"a\qb"` is `a\qb`). In particular `\'` is *not* an escape: write a string containing `'` with double quotes, and one containing `"` with single quotes or `\"`.
- Strings are immutable values; operations return new strings.
- Strings that come from the outside world (files, sockets, processes, HTTP) have NUL bytes silently dropped.

```my
print("tab:\there")
print('say "hi"')
print("say \"hi\"")
print("it's")
print("a\qb")
print(len("é"))
```

```output
tab:	here
say "hi"
say "hi"
it's
a\qb
2
```

## Lists

Ordered, mutable and **shared by reference**: assigning a list to another name or passing it to a function does not copy it.

```my
a = [1, 2]
b = a
push(b, 3)
print(a)
a[0] = 99
print(b)
```

```output
[1, 2, 3]
[99, 2, 3]
```

Create with `[...]` or `range(n)`. Add with `push(list, x)` (end only) or `list + list`. Remove with `pop(list)` (end only). Assign with `xs[i] = v` (index must exist). Copy with `slice(xs, 0, len(xs))`. See [builtins.md](builtins.md).

## Maps

String keys, values of any type, **insertion ordered**, shared by reference. Keys must be strings (`{1: 2}` and `m[1]` are errors); use `text(n)` for numeric keys.

```my
user = {"name": "ada"}
user["age"] = 36
print(user)
print(has(user, "age"))
print(get(user, "email", "none"))
remove(user, "age")
print(len(user))
```

```output
{"name": "ada", "age": 36}
True
none
1
```

Maps nest freely and can be modified in place at any depth: `m["a"]["b"][0] = 7`. Iterating a map with `for` yields its keys in insertion order.

## Statements

### Expression statement
Any expression on its own line (typically a call). Its value is discarded. The statement `None` is a no-op.

### Assignment
`name = expr` binds a variable. `target[index] = expr` stores into a list or map, where `target` is any expression that yields a list/map (`a["k"][2] = v`).
Not supported: `a = b = 1`, `a, b = ...`, `x += 1`, `obj.attr = v`. Write `x = x + 1`.

### `print(expr)`
Prints one value followed by a newline. Strings print raw; lists and maps print their literal form (strings inside are quoted). `print` is a statement, not a function: it takes exactly one argument and cannot be passed around. Print several things with `print(a + " " + text(b))`. Floats print with up to 12 significant digits, always showing `.0` for whole numbers.

```my
print("x")
print(["x", 1, None, True, 2.50])
print(0.1 + 0.2)
print(1 / 3)
print(float("1e15"))
```

```output
x
["x", 1, None, True, 2.5]
0.3
0.333333333333
1e+15
```

### `if` / `elif` / `else`

```my
n = 5
if n < 3:
    print("low")
elif n < 10:
    print("mid")
else:
    print("high")
```

```output
mid
```

### `while`

```my
i = 0
while True:
    i = i + 1
    if i == 2:
        continue
    if i > 4:
        break
    print(i)
```

```output
1
3
4
```

### `for name in iterable:`
Iterates a **list** (elements), a **map** (keys, insertion order) or a **string** (one-byte strings). `range(n)` gives a list of integers. `break` and `continue` work as in `while`. The loop variable keeps its last value after the loop. Appending to a list while iterating it extends the iteration; avoid that.

```my
for i in range(3):
    print(i)
for k in {"a": 1, "b": 2}:
    print(k)
for ch in "hi":
    print(ch)
```

```output
0
1
2
a
b
h
i
```

### `def` and `return`

```my
def area(w, h):
    return w * h

def greet(name):
    if name == "":
        return
    print("hi " + name)

print(area(3, 4))
print(greet(""))
greet("ada")
```

```output
12
None
hi ada
```

Rules:
- Exact argument count is required (`Expected 2 arguments but got 1.`). No default, keyword or variadic arguments.
- `def` may appear only at the top level of the file (or inside a top-level `if`/`while`/`for` block), **not inside another function**.
- A function exists once its `def` line has *executed*; call it only after that.
- A function without `return`, or a bare `return`, gives `None`.
- Functions are values: store them in variables, lists and maps (`ops = {"add": add}; ops["add"](1, 2)`), and call through them.
- Recursion works but the interpreter allows 10000 active calls **including the main program**, so a function can call itself at most 9999 levels deep (`line N: Stack overflow: calls nested too deeply (limit 9999).` beyond that, which is fatal, so unbounded recursion fails quickly with the line of the call). Prefer loops, and build tables like Fibonacci bottom-up instead of recursing from the top.
- The value stack holds 65536 slots in total, so calls with enormous argument lists or deeply nested expressions can overflow it.

### Scope
- At the top level every variable is global.
- Inside a function, the parameters and **every name assigned anywhere in the function body** (including `for` loop variables) are local for the *whole* body. A local that has not been assigned yet reads as `None`.
- Any other name is looked up as a global **at call time** (this includes builtins and imported modules).
- A parameter or local with the same name as a builtin or module **hides it inside that function**. `def wrap(text, width)` makes `text(...)` unusable in the body (calling the string fails with `Can only call functions.`); name it `text_in`. Names to avoid for variables and parameters: `len push pop keys values has get remove type text int float range sort slice args`, and module names such as `str`, `math`, `time`, `path`, `os`, `json`, `re`, `net`, `db`, `sys`, `io`, `fs`.
- Consequence: **a function cannot rebind a global variable.** `x = 1` inside a function creates a new local `x`. But a function *can mutate* a global list or map. To share state, keep it in a map or list, or return values.

When several functions must share mutable data (a parser position, counters, a flag), keep it in one map and pass the map around:

```my
def next_char(st):
    if st["pos"] >= len(st["text"]):
        return None
    c = st["text"][st["pos"]]
    st["pos"] = st["pos"] + 1
    return c

st = {"text": "ab", "pos": 0}
print(next_char(st))
print(next_char(st))
print(next_char(st))
```

```output
a
b
None
```

```my
counter = 0
state = {"n": 0}

def shadow():
    counter = 100
    return counter

def bump():
    state["n"] = state["n"] + 1

print(shadow())
bump()
bump()
print(counter)
print(state["n"])
```

```output
100
0
2
```

Reading a local before assigning it is a classic trap: the whole function treats the name as local, so it is `None`, not the global.

```my-error
counter = 0
def broken():
    counter = counter + 1
    return counter
broken()
```

```output
Unsupported operand types.
```

### `import`
`import name` loads `name.so` from the module search path and binds the global `name`. Use members as `name.func(...)`. There is no `from x import y`, no `import x as y`, and no way to import `.my` files. Module functions can be stored in variables (`up = str.upper`). A module value supports only attribute access.

Search path: `<directory of the script>/modules`, then each directory in the colon-separated `$MPLANGP_MODULES`, then `./modules`.

### `break`, `continue`
Only valid inside `while`/`for` (compile error `'break' outside loop` otherwise). They apply to the innermost loop and do not cross a `def`.

### Not in the language
`pass`, `try`/`except`, `raise`, `class`, `lambda`, `global`, `del`, `assert`, `with`, `async`, tuples, sets, slicing `a[1:2]`, ternary `a if c else b`, `is`, `not in`, `//`, `**`, `+=` and friends, f-strings, default/keyword arguments, `elif` after `else`, nested functions, multiple assignment, string methods (`s.upper()`; use the `str` module).

## Errors and exit codes

There is no way to catch an error. When a runtime error happens the program stops, nothing after it runs, and the interpreter writes `line N: message` to stderr.

| exit code | meaning |
|---|---|
| 0 | success |
| 65 | syntax or compile error (nothing ran), e.g. `line 3: expected ':'` |
| 70 | runtime error, e.g. `line 9: Index out of range.` |
| 1 | usage or the script file could not be read |
| other | whatever `os.exit(code)` was given |

```my-error
xs = [1, 2]
print(xs[5])
```

```output
Index out of range.
```

Runtime messages you will meet: `Undefined variable 'x'.`, `Unsupported operand types.`, `Division by zero.`, `Index out of range.`, `Key 'k' not found.`, `Expected N arguments but got M.`, `Can only call functions.`, `Only modules have attributes.`, `Module 'm' has no attribute 'f'.`, `No module named 'm'.`, `Stack overflow.`, and messages from module functions such as `cannot read '/x': No such file or directory`.

### Syntax errors you will meet

Syntax errors name only the line and the first token the parser could not accept, so match the message to the usual cause:

| message | usual cause |
|---|---|
| `expected ')'` | two arguments to `print(a, b)`; a ternary `a if c else b`; a missing `)`; `x not in y`; a number like `1e3` or `.5` |
| `expected ':'` | missing colon after `if`, `while`, `for`, `def`, `else` |
| `expected newline` | two statements on one line; chained assignment `a = b = 1`; `x = 1 x = 2` |
| `invalid syntax` | `+=`, `//`, `**`, `a[1:2]`, a stray operator, wrong indentation at a statement start |
| `expected an indented block` | a block with no statement after the colon (there is no `pass`; write `None`) |
| `cannot assign to expression` | `obj.field = v` or `f(x) = v` (maps are written with `m["field"] = v`) |
| `tab in indentation` | use spaces |
| `unterminated string` | missing closing quote; strings cannot span lines |
| `'return' outside function`, `'break' outside loop`, `nested def is not supported` | self-explanatory |

Because errors are fatal, **check first**: `fs.exists(p)` before `fs.read(p)`, `has(m, k)` before `m[k]`, `len(xs) > 0` before `pop(xs)`, `proc.which(cmd)` before running a command. Errors from the outside world (network, missing files) also stop the program; use the pre-checks in [cookbook.md](cookbook.md#defensive-patterns), or run risky work in a child process with `proc.run([...])` whose non-zero exit is just data.

stdout is buffered: when both streams go to the same place, stderr text may appear before earlier `print` output. Call `io.flush()` (or `io.eprint`, which flushes first) when ordering matters.

## Limits

| limit | value | failure |
|---|---|---|
| call depth | 10000 frames (the main program is one, so 9999 nested calls) | `Stack overflow: calls nested too deeply (limit 9999).` |
| value stack | 65536 slots | `Stack overflow: value stack full (65536 slots).` |
| distinct constants + names per function body / main program | 65536 | `too many constants` |
| local variables per function | 255 | `too many local variables` |
| arguments per call | 255 | `too many arguments` |
| elements per list/map literal | 255 (use `push` for more) | `list literal too long` |
| nested loops | 32 (16 of them `for`) | `loops nested too deeply` |
| code size of one loop body or `if` branch | about 64 KB of bytecode | `jump too large` |
| `range` size | 10,000,000 | `range is too large.` |
| module imports | 64 | `No module named` |

The interpreter is a simple bytecode VM: expect roughly 10-50 million simple operations per second. Push heavy work into modules (`re`, `json`, `db`, `proc`).

## Command line

```
mplangp file.my [args...]
```
`args()` returns the extra arguments as a list of strings. Standard input is available through the `io` module. The working directory is inherited; paths are relative to it.
