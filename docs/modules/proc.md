# proc: run other programs

`import proc`. Starts a program, waits for it to finish, and returns what it printed. This is the
escape hatch to the whole command-line world (git, curl, ffmpeg, ssh, ...). A command that fails
is **not** an error here: you get its exit code and decide.

| function | returns | notes |
|---|---|---|
| `run(argv, input?)` | map | run a program **without a shell**. `argv` is a non-empty list of strings: program then arguments. Optional `input` string is sent to its stdin |
| `sh(command, input?)` | map | run `command` through `/bin/sh -c` (pipes, globs, `&&`, redirects work) |
| `which(name)` | str or None | full path of an executable found on `$PATH` |

Result map: `{"code": int, "out": str, "err": str}`.

- `code` is the exit status. If the program was killed by a signal it is `-signal` (e.g. `-9`). **`127` means the program could not be started** (not found / not executable).
- `out` and `err` are the full captured stdout and stderr (NUL bytes dropped).
- stdin is closed (EOF) when no `input` is given. The child inherits the environment (see `os.setenv`) and working directory (`fs.chdir`).
- The call blocks until the program exits; there is **no timeout**. Don't run things that wait forever (use the program's own timeout flag, e.g. `["curl", "-m", "10", ...]`, or `["timeout", "10", ...]` where available).
- Prefer `run` with an argument list over `sh` whenever any part comes from data: it cannot be hijacked by quotes or `;` in the data. With `sh`, quote untrusted text yourself.

```my
import proc

r = proc.run(["echo", "hello", "world"])
print(r["code"])
print(r["out"])
print(proc.run(["definitely-not-a-program"])["code"])
```

```output
0
hello world

127
```

```my
import proc

r = proc.sh("echo to-out; echo to-err 1>&2; exit 3")
print(r)
print(proc.sh("tr a-z A-Z", "shout this")["out"])
print(proc.sh("echo one && echo two")["out"] == "one\ntwo\n")
print(proc.which("sh") != None)
print(proc.which("no-such-binary-xyz"))
```

```output
{"code": 3, "out": "to-out\n", "err": "to-err\n"}
SHOUT THIS
True
True
None
```

## Checking success

```my
import proc
import str

def run_ok(argv):
    r = proc.run(argv)
    if r["code"] != 0:
        print("failed (" + text(r["code"]) + "): " + str.trim(r["err"]))
        return None
    return str.trim(r["out"])

print(run_ok(["echo", "fine"]))
print(run_ok(["sh", "-c", "echo broken 1>&2; exit 1"]))
```

```output
fine
failed (1): broken
None
```

Typical uses: `proc.run(["git", "status", "--short"])`, `proc.sh("ls | wc -l")`, `proc.run(["open", url])`. Start with `proc.which("git") != None` to check that a tool exists before depending on it.
