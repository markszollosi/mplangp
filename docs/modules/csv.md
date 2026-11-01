# csv: comma-separated values

`import csv`. Tables as a **list of rows**, each row a **list of strings**.

| function | returns | notes |
|---|---|---|
| `parse(text, sep?)` | list of lists | `sep` is a one-character string (default `","`, use `"\t"` for TSV, `";"`, `"|"`) |
| `dump(rows, sep?)` | str | one line per row, each ending in `\n` |

`parse`:
- Handles quoted fields (`"a,b"`), doubled quotes inside quotes (`"say ""hi"""`), newlines inside quotes, and both `\n` and `\r\n` line endings.
- A final newline does not create an empty row; empty input gives `[]`.
- **Everything stays a string**: convert yourself with `int(...)` / `float(...)`. There is no header handling: the first row is just a row.
- An unterminated quote is an error.

`dump`: cells may be strings, ints, floats, bools (`True`/`False`) or `None` (empty cell). Cells containing the separator, a quote or a line break are quoted.

```my
import csv

rows = csv.parse("name,score\nada,10\n\"smith, bob\",7\n")
print(rows)
total = 0
for i in range(1, len(rows)):
    total = total + int(rows[i][1])
print(total)
```

```output
[["name", "score"], ["ada", "10"], ["smith, bob", "7"]]
17
```

```my
import csv

out = csv.dump([["id", "note"], [1, "plain"], [2, "has, comma"], [3, "quote \" mark"], [4, None]])
print(out)
print(csv.parse("a;b\n1;2\n", ";"))
```

```output
id,note
1,plain
2,"has, comma"
3,"quote "" mark"
4,

[["a", "b"], ["1", "2"]]
```

Rows to maps using the first row as header:

```my
import csv

rows = csv.parse("name,age\nada,36\nbob,41\n")
header = rows[0]
records = []
for i in range(1, len(rows)):
    rec = {}
    for j in range(len(header)):
        rec[header[j]] = rows[i][j]
    push(records, rec)
print(records)
```

```output
[{"name": "ada", "age": "36"}, {"name": "bob", "age": "41"}]
```
