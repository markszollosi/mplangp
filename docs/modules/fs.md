# fs: files and directories

`import fs`. Reads and writes files, lists directories, creates and removes things.
All paths are strings, relative to the current working directory unless absolute. Every failure
is a fatal runtime error whose message contains the path and the system reason, for example
`cannot read '/nope': No such file or directory`, so test with `fs.exists` first.

| function | returns | notes |
|---|---|---|
| `read(path)` | str | whole file as text; NUL bytes are dropped (binary files are not supported) |
| `write(path, text)` | None | create or **truncate**; the parent directory must exist |
| `append(path, text)` | None | create or append |
| `exists(path)` | bool | file, directory or anything else; follows symlinks |
| `isfile(path)` | bool | regular file |
| `isdir(path)` | bool | directory |
| `stat(path)` | map | `{"size": int bytes, "mtime": int epoch seconds, "mode": int permission bits, "dir": bool}` |
| `list(dir)` | list of str | entry **names** (not paths), sorted, without `.` and `..` |
| `glob(pattern)` | list of str | paths matching a shell pattern (`*`, `?`, `[..]`), sorted; empty list if none |
| `mkdir(path)` | None | creates missing parents too; fine if it already exists |
| `remove(path)` | None | deletes a file or an **empty** directory; error if missing |
| `rename(from, to)` | None | move/rename |
| `copy(from, to)` | None | copy one file's bytes (not directories) |
| `cwd()` | str | current working directory |
| `chdir(path)` | None | change working directory for the rest of the program |

`stat` `mode` is the permission number, so `420` is octal `0644` and `493` is `0755`.

## Examples

```my
import fs
import path

base = "/tmp/mplangp_doc_fs"
fs.mkdir(path.join(base, "logs"))
fs.write(path.join(base, "notes.txt"), "line one\n")
fs.append(path.join(base, "notes.txt"), "line two\n")
print(fs.read(path.join(base, "notes.txt")))
print(fs.list(base))
print(fs.stat(path.join(base, "notes.txt"))["size"])
print(fs.exists(path.join(base, "missing")))
fs.copy(path.join(base, "notes.txt"), path.join(base, "logs", "copy.txt"))
print(fs.glob(base + "/logs/*.txt") == [base + "/logs/copy.txt"])
fs.remove(path.join(base, "logs", "copy.txt"))
fs.remove(path.join(base, "logs"))
fs.remove(path.join(base, "notes.txt"))
fs.remove(base)
print(fs.exists(base))
```

```output
line one
line two

["logs", "notes.txt"]
18
False
True
False
```

(The blank line after `line two` is the file's final newline followed by `print`'s own.)

Read a file line by line with `str.lines`:

```my
import fs
import str

fs.write("/tmp/mplangp_doc_fs2.txt", "a\nb\nc\n")
for line in str.lines(fs.read("/tmp/mplangp_doc_fs2.txt")):
    print("> " + line)
fs.remove("/tmp/mplangp_doc_fs2.txt")
```

```output
> a
> b
> c
```

## Recursive removal and walking

There is no recursive delete or tree walk. Do it with a function:

```my
import fs
import path

def rmtree(p):
    if fs.isdir(p):
        for name in fs.list(p):
            rmtree(path.join(p, name))
    fs.remove(p)

def walk(p, found):
    for name in fs.list(p):
        full = path.join(p, name)
        if fs.isdir(full):
            walk(full, found)
        else:
            push(found, full)

fs.mkdir("/tmp/mplangp_doc_tree/a/b")
fs.write("/tmp/mplangp_doc_tree/a/b/x.txt", "x")
fs.write("/tmp/mplangp_doc_tree/y.txt", "y")
files = []
walk("/tmp/mplangp_doc_tree", files)
print(files)
rmtree("/tmp/mplangp_doc_tree")
print(fs.exists("/tmp/mplangp_doc_tree"))
```

```output
["/tmp/mplangp_doc_tree/a/b/x.txt", "/tmp/mplangp_doc_tree/y.txt"]
False
```

For large copies, moves across devices or permissions changes, run a command: `proc.run(["cp", "-r", a, b])`.
