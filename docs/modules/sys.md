# sys: machine facts

`import sys`. Read-only information about the computer. Values differ per machine, so use them
for decisions and reports, not for exact output.

| function | returns | notes |
|---|---|---|
| `cpus()` | int | online logical processors |
| `load()` | list of 3 floats | load average over 1, 5 and 15 minutes |
| `uptime()` | float | seconds since boot |
| `memory()` | map | `{"total": bytes, "free": bytes}`; `free` is free plus reclaimable memory and may be `None` if the OS refuses to say |
| `disk(path)` | map | filesystem containing `path`: `{"total", "free", "used"}` in bytes (`free` is what an ordinary user may use) |
| `uname()` | map | `{"system", "node", "release", "version", "machine"}` |

```my
import sys

print(sys.cpus() >= 1)
print(len(sys.load()))
print(sys.uptime() > 0)
print(sys.memory()["total"] > 0)
d = sys.disk("/")
print(d["free"] <= d["total"])
print(sys.uname()["system"] != "")
```

```output
True
3
True
True
True
True
```

Percent of disk used:

```my
import sys

d = sys.disk("/")
pct = d["used"] * 100 / d["total"]
print(pct >= 0 and pct <= 100)
```

```output
True
```

Slow or detailed hardware questions (battery, temperature, GPU) are answered by running the platform tool: `proc.sh("pmset -g batt")` on macOS, files under `/sys` on Linux.
