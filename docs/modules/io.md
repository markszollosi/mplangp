# io: terminal, stdin and stderr

`import io`. Talking to the person (or pipe) on the other side of the program. `print` already
covers normal output; use `io` for input, output without a newline, stderr and colours.

| function | returns | notes |
|---|---|---|
| `input(prompt?)` | str or None | print the prompt (if given) and read one line from stdin without its line ending; **`None` at end of input** |
| `readall()` | str | everything left on stdin (empty string if none) |
| `write(text)` | None | write to stdout **without** a newline (use `"\n"` yourself) |
| `eprint(text)` | None | write a line to **stderr** (flushes stdout first so ordering is kept) |
| `flush()` | None | flush buffered stdout now |
| `isatty(fd?)` | bool | whether `fd` (default 1, stdout) is a terminal. Use `2` for stderr, `0` for stdin |
| `size()` | map | terminal `{"cols", "rows"}`; `80` x `24` when stdout is not a terminal |
| `color(name, text)` | str | wrap text in ANSI escape codes |

`color` names: `reset bold dim underline black red green yellow blue magenta cyan white gray`.
It **always** adds the codes; check `io.isatty()` if the output may be redirected to a file.
Unknown names are an error.

stdout is buffered (flushed at exit, on `flush`, `eprint`, `sleep`, `os.exit` and before reading input from a prompt). If your program writes a progress line with `io.write`, call `io.flush()` so it appears immediately.

```my
import io

io.write("no newline, ")
io.write("then newline\n")
io.flush()
print(io.color("red", "error") == "\e[31merror\e[0m")
print(io.size()["cols"] > 0)
print(io.isatty(0))
```

```output
no newline, then newline
True
True
False
```

Reading input (stdin is empty when the examples in these docs run, so the interactive ones are not executed):

```my-norun
import io

name = io.input("Your name? ")
if name == None:
    print("no input")
else:
    print("Hello " + name)
```

Read all lines of piped input (`cat data.txt | mplangp prog.my`):

```my-norun
import io
import str

for line in str.lines(io.readall()):
    print(str.upper(line))
```

With no stdin attached the readers see end of input immediately:

```my
import io

print(io.input())
print(io.readall() == "")
```

```output
None
True
```

Diagnostics belong on stderr so they don't pollute output that other programs consume: `io.eprint("warning: ...")`.
