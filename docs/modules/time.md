# time: clocks, sleeping, dates

`import time`. Timestamps are numbers of seconds since 1970-01-01 UTC (the Unix epoch): `float`
from the clocks, `int` from `parse`.

| function | returns | notes |
|---|---|---|
| `now()` | float | wall-clock time, microsecond resolution |
| `mono()` | float | monotonic clock for measuring durations (arbitrary start) |
| `sleep(seconds)` | None | pause; `0` to `86400`, fractions allowed. Flushes stdout first |
| `format(ts, fmt?, utc?)` | str | `strftime` text. Default `fmt` is `"%Y-%m-%d %H:%M:%S"`; `utc` (bool, default `False`) selects UTC instead of the local time zone |
| `iso(ts)` | str | UTC ISO-8601: `"2001-09-09T01:46:40Z"` |
| `parse(text, fmt?, utc?)` | int | inverse of `format` (`strptime`); the **whole text must match**. Local time zone unless `utc` is `True` |
| `parts(ts, utc?)` | map | `year month day hour minute second weekday yearday`; `weekday` is `0` = Sunday, `yearday` starts at 1 |

`iso` is **always UTC** (note the `Z`). For the local date or time use `format` without the `utc` flag: `time.format(time.now(), "%Y-%m-%d")` is today's date in the machine's time zone, and `time.format(time.now(), "%Y-%m-%dT%H:%M:%S%z")` adds the offset.

Common `fmt` codes: `%Y` year, `%m` month, `%d` day, `%H` `%M` `%S` time, `%A` weekday name, `%B` month name, `%j` day of year, `%z` offset, `%s` epoch seconds (platform dependent).
Pass `True` for `utc` in programs whose output must not depend on the machine's time zone.

```my
import time

print(time.format(0, "%Y-%m-%d %H:%M:%S", True))
print(time.iso(1000000000))
print(time.parse("2001-09-09 01:46:40", "%Y-%m-%d %H:%M:%S", True))
print(time.parts(1000000000, True))
print(time.format(1000000000, "%A %d %B %Y", True))
```

```output
1970-01-01 00:00:00
2001-09-09T01:46:40Z
1000000000
{"year": 2001, "month": 9, "day": 9, "hour": 1, "minute": 46, "second": 40, "weekday": 0, "yearday": 252}
Sunday 09 September 2001
```

Measuring and waiting:

```my
import time

start = time.mono()
time.sleep(0.05)
elapsed = time.mono() - start
print(elapsed >= 0.05 and elapsed < 1)
print(time.now() > 1700000000)
```

```output
True
True
```

Date arithmetic is plain arithmetic on seconds: tomorrow is `ts + 86400` (ignoring DST when using local time; use `utc=True` to be exact).
