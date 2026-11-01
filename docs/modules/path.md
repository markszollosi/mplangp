# path: path strings

`import path`. Pure string manipulation using `/` separators; it never touches the disk (except
`abs`, which reads the working directory). Combine with [fs](fs.md).

| function | returns | notes |
|---|---|---|
| `join(a, b, ...)` | str | one or more strings; inserts `/` as needed; an **absolute later part discards earlier ones**; empty parts are skipped |
| `base(p)` | str | last component: `"/x/y/z.tar.gz"` gives `"z.tar.gz"` |
| `dir(p)` | str | everything before the last component; `"."` if none, `"/"` for the root |
| `ext(p)` | str | extension of the last component including the dot, `""` if none (`.bashrc` has none) |
| `stem(p)` | str | last component without its final extension |
| `norm(p)` | str | collapses `//`, `.` and `a/..`; leading `..` of relative paths are kept; empty result is `"."` |
| `abs(p)` | str | `norm` of the path made absolute against the working directory (symlinks are not resolved, the path need not exist) |
| `home()` | str or None | `$HOME` |

```my
import path

print(path.join("a", "b", "c.txt"))
print(path.join("a", "/abs", "x"))
print(path.base("/x/y/z.tar.gz"))
print(path.dir("/x/y/z.tar.gz"))
print(path.dir("file"))
print(path.ext("/x/y/z.tar.gz"))
print(path.stem("/x/y/z.tar.gz"))
print(path.norm("a/./b/../c//d"))
print(path.norm("../a/../.."))
print(path.abs("/a/b/../c"))
```

```output
a/b/c.txt
/abs/x
z.tar.gz
/x/y
.
.gz
z.tar
a/c/d
../..
/a/c
```

Always build paths with `path.join` rather than string `+` to avoid doubled or missing slashes.
Windows paths are not supported.
