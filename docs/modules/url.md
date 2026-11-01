# url: split and build URLs

`import url`. Pure string handling, no network access.

| function | returns | notes |
|---|---|---|
| `parse(u)` | map | split a URL into parts (below) |
| `query(map)` | str | build `a=1&b=x%20y` from a map |

`parse` result keys: `scheme`, `user` (the `user:password` part, `""` if none), `host`, `port` (**int or `None`**), `path`, `query` (raw string without `?`), `fragment` (without `#`), and `params`: the query decoded into a map (`%XX` and `+` decoded; a repeated key keeps the last value; a key without `=` gets `""`).
Missing parts are `""` (and `None` for the port). A relative reference like `/a/b?x=1` parses with empty scheme and host. A non-numeric port is an error.

`query` values may be strings, ints, floats, bools (`"true"`/`"false"`) or `None` (empty); keys and values are percent-encoded (everything except letters, digits and `-_.~`).

```my
import url

u = url.parse("https://user:pw@example.com:8080/a/b?x=1&y=a%20b&z#frag")
print(u["host"])
print(u["port"])
print(u["path"])
print(u["params"])
print(u["fragment"])
print(url.parse("http://example.com/")["port"])
print(url.parse("/just/a/path?q=1")["path"])
print(url.query({"a": "b c", "n": 3, "ok": True, "empty": None}))
```

```output
example.com
8080
/a/b
{"x": "1", "y": "a b", "z": ""}
frag
None
/just/a/path
a=b%20c&n=3&ok=true&empty=
```

Default ports (`80`/`443`) are not filled in: `port` is `None` when the URL has none. Rebuild a URL yourself:

```my
import url

u = url.parse("https://example.com/search?q=old")
new = u["scheme"] + "://" + u["host"] + u["path"] + "?" + url.query({"q": "new value", "page": 2})
print(new)
```

```output
https://example.com/search?q=new%20value&page=2
```
