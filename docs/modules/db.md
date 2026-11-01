# db: SQLite databases

`import db`. A full SQL database in a single file (or in memory). Good for anything you would
otherwise keep in many lists: queries, joins, grouping, persistence between runs. A database is an
**integer handle**; up to 16 can be open at once.

| function | returns | notes |
|---|---|---|
| `open(path)` | int | open or create a database file; `":memory:"` gives a temporary one that disappears when closed |
| `close(db)` | None | |
| `exec(db, sql, params?)` | int | run a statement that changes data; returns the number of rows changed. Without `params` the text may contain **several statements** (a script). With `params` it must be a single statement |
| `query(db, sql, params?)` | list of maps | run a `SELECT` (or anything returning rows); each row is a map of column name to value. Single statement only |
| `last_id(db)` | int | rowid of the most recent `INSERT` |

**Always pass values as `params`** (a list matching the `?` placeholders in order) instead of pasting them into the SQL text. This prevents SQL injection and quoting bugs. The number of parameters must equal the number of `?`. Parameter types: `None` (NULL), bool, int, float, str.

Result values: SQLite INTEGER is int, REAL is float, TEXT is str, NULL is `None` (blobs come back as text). If two result columns share a name, the later one wins; alias them (`select a.id as a_id, b.id as b_id`). Errors are fatal: `sql error: <sqlite message>`.

```my
import db

h = db.open(":memory:")
db.exec(h, "create table people (id integer primary key, name text, score real)")
print(db.exec(h, "insert into people (name, score) values (?, ?)", ["ada", 9.5]))
db.exec(h, "insert into people (name, score) values (?, ?)", ["bob", None])
print(db.last_id(h))
print(db.query(h, "select * from people order by id"))
print(db.query(h, "select name from people where score > ?", [5]))
print(db.query(h, "select count(*) as n, max(score) as best from people")[0])
print(db.exec(h, "update people set score = 0 where score is null"))
db.close(h)
```

```output
1
2
[{"id": 1, "name": "ada", "score": 9.5}, {"id": 2, "name": "bob", "score": None}]
[{"name": "ada"}]
{"n": 2, "best": 9.5}
1
```

Scripts and transactions (group many writes so they are atomic and fast):

```my
import db

h = db.open(":memory:")
db.exec(h, "create table log (n integer); insert into log values (0); insert into log values (1);")
db.exec(h, "begin")
for i in range(2, 100):
    db.exec(h, "insert into log values (?)", [i])
db.exec(h, "commit")
print(db.query(h, "select count(*) as n from log")[0]["n"])
db.close(h)
```

```output
100
```

Persistence between runs:

```my
import db
import fs

path = "/tmp/mplangp_doc.sqlite"
h = db.open(path)
db.exec(h, "create table if not exists kv (k text primary key, v text)")
db.exec(h, "insert or replace into kv values (?, ?)", ["greeting", "hello"])
db.close(h)

h = db.open(path)
print(db.query(h, "select v from kv where k = ?", ["greeting"])[0]["v"])
db.close(h)
fs.remove(path)
```

```output
hello
```

Common mistakes: using `db.exec` for a `SELECT` (it runs but discards rows; use `query`), forgetting the `params` list for a statement that contains `?` (`statement needs 1 parameters but got 0.`), and pasting user text into SQL.
