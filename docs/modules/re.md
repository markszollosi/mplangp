# re: regular expressions

`import re`. POSIX **extended** regular expressions (the same as `grep -E`). Matching is
byte-based and finds a match anywhere in the text unless anchored with `^` / `$`.

**What is not supported** (all fail to match or error): `\d`, `\w`, `\s`, `\b`, non-greedy `*?`, lookahead/lookbehind, named groups, back-references inside the pattern, case-insensitive flags.
Use POSIX classes instead: `[[:digit:]]`, `[[:alpha:]]`, `[[:alnum:]]`, `[[:space:]]`, `[[:upper:]]`, `[[:lower:]]`, `[[:punct:]]`; and `[Aa]` for case folding.
Escape a literal special character with a backslash, and remember string literals need that backslash doubled: `"\\."` is the pattern `\.` (a literal dot).

| function | returns | notes |
|---|---|---|
| `test(pattern, text)` | bool | does the pattern match anywhere |
| `find(pattern, text)` | list or None | first match as `[whole, group1, group2, ...]` (up to 9 groups); a group that did not take part is `None`; `None` if no match |
| `findall(pattern, text)` | list of str | every non-overlapping whole match |
| `sub(pattern, replacement, text)` | str | replace **all** matches. In `replacement`, `\0` is the whole match, `\1`..`\9` are groups, `\\` is a backslash |
| `split(pattern, text)` | list of str | split at each match (empty matches are ignored) |

A bad pattern is a fatal error `bad pattern: <reason>`.

```my
import re

print(re.test("^[[:digit:]]+$", "12345"))
print(re.test("^[[:digit:]]+$", "12a45"))
print(re.find("([a-z]+)@([a-z]+)\\.com", "mail bob@site.com now"))
print(re.find("x", "abc"))
print(re.findall("[[:digit:]]+", "a1 b22 c333"))
print(re.sub("([a-z]+)=([0-9]+)", "\\2:\\1", "a=1, bc=22"))
print(re.split("[ ,]+", "a, b  c,d"))
print(re.sub("[[:space:]]+", " ", "too    many\t\tspaces"))
```

```output
True
False
["bob@site.com", "bob", "site"]
None
["1", "22", "333"]
1:a, 22:bc
["a", "b", "c", "d"]
too many spaces
```

Extracting fields:

```my
import re

line = "2026-11-01 ERROR disk=/dev/sda1 usage=93%"
m = re.find("^([0-9-]+) ([A-Z]+) disk=([^ ]+) usage=([0-9]+)%", line)
if m != None:
    print(m[2] + " on " + m[3] + " at " + m[4])
emails = re.findall("[[:alnum:]._]+@[[:alnum:].]+[[:alpha:]]", "ann@x.org, bob.b@y.co.uk; nope")
print(emails)
```

```output
ERROR on /dev/sda1 at 93
["ann@x.org", "bob.b@y.co.uk"]
```

For plain substrings, `in`, `str.find` and `str.replace` are simpler and faster than a regex.
