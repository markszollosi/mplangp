# os: process environment and identity

`import os`. Environment variables, host identity, and process exit.

| function | returns | notes |
|---|---|---|
| `env(name)` | str or None | `None` when unset |
| `setenv(name, value)` | None | affects this process and the programs it starts later (`proc`) |
| `unsetenv(name)` | None | |
| `environ()` | map | all variables as `{name: value}` |
| `platform()` | str | lower-case kernel name: `"darwin"`, `"linux"` |
| `arch()` | str | machine type: `"arm64"`, `"x86_64"` |
| `hostname()` | str | |
| `user()` | str or None | login name of the current user |
| `pid()` | int | this process id |
| `exit(code=0)` | never | flushes stdout and ends the program immediately with that exit status |

Command-line arguments are not here: use the builtin `args()` ([builtins.md](../builtins.md)).

```my
import os

print(os.env("MPLANGP_DOC_UNSET"))
os.setenv("MPLANGP_DOC_VAR", "hello")
print(os.env("MPLANGP_DOC_VAR"))
print("MPLANGP_DOC_VAR" in os.environ())
os.unsetenv("MPLANGP_DOC_VAR")
print(os.env("MPLANGP_DOC_VAR"))
print(type(os.pid()))
```

```output
None
hello
True
None
int
```

A script that signals failure to its caller:

```my-norun
import os
import fs
import io

if not fs.exists("config.json"):
    io.eprint("config.json is missing")
    os.exit(2)
```

```my
import os
print("about to exit")
os.exit(0)
print("never printed")
```

```output
about to exit
```
