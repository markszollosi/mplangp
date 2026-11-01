# Builtin functions

Available everywhere without `import`. They are ordinary globals, so a variable **or function parameter** with the same
name hides them (don't call a variable or parameter `len`, `keys`, `type`, `text`, `get`, `int`...; inside `def f(text)` the builtin `text()` is gone).
Wrong argument counts or types are runtime errors with a message naming the function.

| function | returns | summary |
|---|---|---|
| `len(x)` | int | length of a string (bytes), list or map |
| `push(list, x)` | None | append to the end of a list (in place) |
| `pop(list)` | any | remove and return the last element (error if empty) |
| `keys(map)` | list of str | keys in insertion order |
| `values(map)` | list | values in insertion order |
| `has(map, key)` | bool | whether the key exists |
| `get(map, key, default)` | any | value, or `default` when the key is missing (all 3 arguments required) |
| `remove(map, key)` | None | delete a key (no error if absent) |
| `type(x)` | str | `"none" "bool" "int" "float" "str" "list" "map" "function" "module"` |
| `text(x)` | str | text form of any value (strings unchanged, lists/maps in literal form) |
| `int(x)` | int | from int/bool, float (truncates toward zero) or a decimal string |
| `float(x)` | float | from a number or a numeric string (`"1e3"` works) |
| `range(n)` / `range(a, b)` | list of int | `0..n-1` / `a..b-1`, empty if `b <= a`, at most 10,000,000 items, no step |
| `sort(list)` | None | sort ascending **in place**; all numbers or all strings |
| `slice(x, from, to)` | list or str | sub-list/substring `[from, to)`; negative counts from the end; out-of-range clamps |
| `args()` | list of str | command-line arguments after the script path |

```my
xs = [3, 1, 2]
push(xs, 0)
sort(xs)
print(xs)
print(pop(xs))
print(len(xs))
print(slice(xs, 1, -1))
print(slice("hello", 1, 3))
```

```output
[0, 1, 2, 3]
3
3
[1]
el
```

```my
m = {"b": 2, "a": 1}
print(keys(m))
print(values(m))
print(get(m, "zzz", "fallback"))
remove(m, "b")
print(m)
print(has(m, "a"))
```

```output
["b", "a"]
[2, 1]
fallback
{"a": 1}
True
```

## Conversions

```my
print(int("42") + 1)
print(int(7.9))
print(int(-7.9))
print(int(True))
print(float("2.5") * 2)
print(float(3))
print(text(3.0) + "|" + text(None) + "|" + text([1, "a"]))
print(type(1) + " " + type(1.0) + " " + type("s") + " " + type(None) + " " + type([]) + " " + type({}))
```

```output
43
7
-7
1
5.0
3.0
3.0|None|[1, "a"]
int float str none list map
```

`int("abc")` and `int("1.5")` are errors (`int(x) cannot parse the string.`): use `float` first (`int(float("1.5"))`). Leading whitespace is allowed in `int("  42")`, trailing is not.

## Things that do not exist (and what to do instead)

| missing | instead |
|---|---|
| list reverse / insert / delete at index / `index()` / `count()` | build a new list with a `for` loop and `push` |
| `sum`, `min`, `max`, `abs`, `round` builtins | `math.min`, `math.max`, `math.abs`, `math.round` or a loop |
| sorting by key or descending | `sort`, then loop backwards with `range`; or sort a list of keys |
| string methods | `import str` (`str.split`, `str.join`, `str.replace`, `str.trim`, ...) |
| number formatting (`%.2f`), padding | `str.fixed(x, 2)`, `str.pad_left`, `str.pad_right` |
| `enumerate`, `zip` | `for i in range(len(xs)):` and index |
| copy of a list/map | `slice(xs, 0, len(xs))`; for a map loop over `keys(m)` |

```my
xs = [1, 2, 3]
rev = []
for i in range(len(xs)):
    push(rev, xs[len(xs) - 1 - i])
print(rev)

copy = {}
original = {"a": 1, "b": 2}
for k in original:
    copy[k] = original[k]
copy["a"] = 99
print(original["a"])
```

```output
[3, 2, 1]
1
```
