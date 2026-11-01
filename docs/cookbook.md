# Cookbook

Complete, tested patterns. Reference pages: [language.md](language.md), [builtins.md](builtins.md), [modules](modules/README.md).

## Defensive patterns

Because there are no exceptions, test the precondition instead of catching the failure.

```my
import fs
import json
import proc

# 1. file may not exist
def read_or(path, default):
    if fs.exists(path) and fs.isfile(path):
        return fs.read(path)
    return default

# 2. map key may not exist
def setting(cfg, name, default):
    return get(cfg, name, default)

# 3. text may not be valid JSON: ask a child process first
def valid_json(text):
    if proc.which("python3") == None:
        return True
    return proc.run(["python3", "-m", "json.tool"], text)["code"] == 0

# 4. list may be empty
def last_or(xs, default):
    if len(xs) == 0:
        return default
    return xs[len(xs) - 1]

print(read_or("/tmp/mplangp_no_such_file", "fallback"))
print(setting({"a": 1}, "b", 0))
print(valid_json("{\"ok\": true}"))
print(valid_json("{broken"))
print(last_or([], "none"))
print(last_or([1, 2], "none"))
```

```output
fallback
0
True
False
none
2
```

Run a risky external task as a child process so its failure is just data:

```my
import proc

r = proc.run(["sh", "-c", "exit 4"])
if r["code"] != 0:
    print("child failed with " + text(r["code"]))
```

```output
child failed with 4
```

Retry loop around a command:

```my
import proc
import time

def retry(argv, attempts):
    for i in range(attempts):
        r = proc.run(argv)
        if r["code"] == 0:
            return r["out"]
        time.sleep(0.01)
    return None

print(retry(["true"], 3) == "")
print(retry(["false"], 3))
```

```output
True
None
```

## Shared state without globals

A function cannot rebind a global, but it can mutate a map or list.

```my
state = {"calls": 0, "seen": []}

def visit(name):
    state["calls"] = state["calls"] + 1
    push(state["seen"], name)

visit("a")
visit("b")
print(state)
```

```output
{"calls": 2, "seen": ["a", "b"]}
```

Memoization with a map (keys must be strings):

```my
memo = {}

def fib(n):
    if n < 2:
        return n
    key = text(n)
    if has(memo, key):
        return memo[key]
    value = fib(n - 1) + fib(n - 2)
    memo[key] = value
    return value

print(fib(40))
```

```output
102334155
```

(Recursion depth is limited to 64 calls; convert deeper recursion into loops.)

## Sorting records

`sort` only sorts all-numbers or all-strings. To order maps, sort an array of keys and look records up:

```my
people = [{"name": "cy", "age": 30}, {"name": "ann", "age": 25}, {"name": "bob", "age": 41}]

by_name = {}
for p in people:
    by_name[p["name"]] = p
names = keys(by_name)
sort(names)
for n in names:
    print(n + " " + text(by_name[n]["age"]))

def sort_by(items, field):
    out = []
    for item in items:
        i = len(out)
        while i > 0 and out[i - 1][field] > item[field]:
            i = i - 1
        # insert at position i by rebuilding the tail
        tail = slice(out, i, len(out))
        out = slice(out, 0, i) + [item] + tail
    return out

print(sort_by(people, "age")[0]["name"])
```

```output
ann 25
bob 41
cy 30
ann
```

## Formatting output

There is no `format`; pad with `*` and `len`.

```my
def pad(s, width):
    s = text(s)
    return s + " " * (width - len(s))

rows = [["name", "qty"], ["apple", 3], ["fig", 12]]
for r in rows:
    print(pad(r[0], 8) + pad(r[1], 4) + "|")
```

```output
name    qty |
apple   3   |
fig     12  |
```

## Command-line program with arguments

```my-norun
import os
import io

a = args()
if len(a) != 2:
    io.eprint("usage: mplangp add.my X Y")
    os.exit(2)
print(int(a[0]) + int(a[1]))
```

Run: `./mplangp add.my 2 3` prints `5`. Arguments are always strings; convert with `int` / `float`.

## Word frequency of a file

```my
import fs
import str

fs.write("/tmp/mplangp_doc_words.txt", "the cat and the hat and the bat\n")
counts = {}
for word in str.split(str.trim(fs.read("/tmp/mplangp_doc_words.txt")), " "):
    if has(counts, word):
        counts[word] = counts[word] + 1
    else:
        counts[word] = 1
words = keys(counts)
sort(words)
for w in words:
    print(w + ": " + text(counts[w]))
fs.remove("/tmp/mplangp_doc_words.txt")
```

```output
and: 2
bat: 1
cat: 1
hat: 1
the: 3
```

## Log analysis with regex and SQL

```my
import re
import db
import str

log = "2026-11-01 ERROR disk full\n2026-11-01 INFO ok\n2026-11-02 ERROR timeout\n"
h = db.open(":memory:")
db.exec(h, "create table events (day text, level text, message text)")
for line in str.lines(log):
    m = re.find("^([0-9-]+) ([A-Z]+) (.*)$", line)
    if m != None:
        db.exec(h, "insert into events values (?, ?, ?)", [m[1], m[2], m[3]])

for row in db.query(h, "select day, count(*) as n from events where level = ? group by day order by day", ["ERROR"]):
    print(row["day"] + ": " + text(row["n"]))
db.close(h)
```

```output
2026-11-01: 1
2026-11-02: 1
```

## Is a service reachable? (port check)

```my
import net

server = net.listen(0)
port = net.port(server)
print(net.open("127.0.0.1", port, 1))
net.close(server)
print(net.open("127.0.0.1", port, 1))
```

```output
True
False
```

## System report

```my
import sys
import os
import time

report = {
    "host": os.hostname(),
    "cpus": sys.cpus(),
    "load1": sys.load()[0],
    "disk_used_pct": sys.disk("/")["used"] * 100 / sys.disk("/")["total"],
    "when": time.iso(time.now()),
}
print(len(keys(report)))
print(report["cpus"] >= 1)
```

```output
5
True
```

## Fetch JSON from the web and save part of it

Needs a network, so shown only:

```my-norun
import http
import json
import fs

r = http.get("https://api.github.com/repos/torvalds/linux")
if r["ok"]:
    repo = json.parse(r["body"])
    fs.write("/tmp/linux.json", json.dump({"stars": repo["stargazers_count"], "lang": repo["language"]}, 2))
    print("saved")
else:
    print("HTTP " + text(r["status"]))
```

## Todo list stored in SQLite

```my
import db
import fs

path = "/tmp/mplangp_doc_todo.sqlite"
h = db.open(path)
db.exec(h, "create table if not exists todo (id integer primary key, title text, done integer default 0)")

def add(title):
    db.exec(h, "insert into todo (title) values (?)", [title])
    return db.last_id(h)

def finish(id):
    db.exec(h, "update todo set done = 1 where id = ?", [id])

first = add("write docs")
add("ship it")
finish(first)
for row in db.query(h, "select id, title, done from todo order by id"):
    mark = " "
    if row["done"] == 1:
        mark = "x"
    print("[" + mark + "] " + row["title"])
db.close(h)
fs.remove(path)
```

```output
[x] write docs
[ ] ship it
```

## Poll until a condition holds, with a deadline

```my
import time
import fs

fs.write("/tmp/mplangp_doc_ready", "yes")
deadline = time.mono() + 2
ready = False
while time.mono() < deadline:
    if fs.exists("/tmp/mplangp_doc_ready"):
        ready = True
        break
    time.sleep(0.05)
print(ready)
fs.remove("/tmp/mplangp_doc_ready")
```

```output
True
```
