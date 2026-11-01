# str: string functions

`import str`. The language has no string methods, so text processing lives here. Strings are
byte sequences; `len`, `find` and indexes count **bytes** (non-ASCII UTF-8 characters take several).
`upper` and `lower` change ASCII letters only. All functions return new strings; nothing is modified in place.

Also useful from the language itself: `+` (concatenate), `*` (repeat), `in` (substring test), `s[i]` (one byte), `slice(s, a, b)` (substring), `len(s)`, `text(x)` (convert), `int(s)`, `float(s)`.

| function | returns | notes |
|---|---|---|
| `len(s)` | int | byte length (same as the builtin `len`) |
| `upper(s)` / `lower(s)` | str | ASCII case conversion |
| `trim(s)` | str | remove whitespace (space, tab, newline, ...) at both ends |
| `split(s, sep)` | list of str | split at every occurrence of the non-empty `sep`; adjacent separators give empty strings; `sep` of `""` splits into single bytes |
| `lines(s)` | list of str | split at `\n` (a `\r` before it is removed); a trailing newline does not add an empty last line |
| `join(list, sep)` | str | concatenate with `sep` between; items may be str, int, float, bool or None (converted like `text`) |
| `replace(s, old, new)` | str | replace **every** occurrence; `old` must be non-empty |
| `find(s, needle)` | int | byte index of the first occurrence, or `-1` |
| `starts(s, prefix)` / `ends(s, suffix)` | bool | |

```my
import str

print(str.upper("shout") + str.lower(" QUIET "))
print("[" + str.trim("  padded \n") + "]")
print(str.split("a,b,,c", ","))
print(str.split("abc", ""))
print(str.lines("one\ntwo\r\nthree\n"))
print(str.join(["x", 1, 2.5, True, None], "-"))
print(str.replace("a.b.c", ".", "::"))
print(str.find("hello", "ll"))
print(str.find("hello", "zz"))
print(str.starts("hello", "he"))
print(str.ends("hello", "lo"))
```

```output
SHOUT quiet 
[padded]
["a", "b", "", "c"]
["a", "b", "c"]
["one", "two", "three"]
x-1-2.5-True-None
a::b::c
2
-1
True
True
```

Recipes:

```my
import str

line = "  key = some value  "
parts = str.split(str.trim(line), "=")
print(str.trim(parts[0]) + "|" + str.trim(parts[1]))

name = "world"
print(str.join(["Hello,", name + "!"], " "))
print("-" * 10)
print(str.find("path/to/file.txt", "/") >= 0)
print(slice("path/to/file.txt", str.find("path/to/file.txt", "file"), 100))
```

```output
key|some value
Hello, world!
----------
True
file.txt
```

For anything pattern-like (digits, optional parts, many alternatives) use [re](re.md).
