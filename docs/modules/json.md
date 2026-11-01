# json: parse and produce JSON

`import json`. Converts between JSON text and language values. The mapping is direct:

| JSON | language value |
|---|---|
| object | map (key order kept; a repeated key keeps the last) |
| array | list |
| string | str |
| number without `.`/exponent that fits 64 bits | int |
| any other number | float |
| `true` / `false` | bool |
| `null` | None |

| function | returns | notes |
|---|---|---|
| `parse(text)` | value | whole text must be one JSON value (surrounding whitespace allowed). Errors: `invalid JSON: <reason>` |
| `dump(value, indent?)` | str | `indent` `0` (default) is compact (`{"a":1,"b":[2]}`), `1`..`16` pretty-prints with that many spaces |

Details:
- `\uXXXX` escapes (including surrogate pairs) decode to UTF-8. `\u0000` becomes `?` because strings cannot hold NUL.
- Nesting deeper than 200 levels is rejected (also protects `dump` against cycles).
- `dump` keeps non-ASCII text as UTF-8, escapes `" \ \n \r \t` and control characters, writes floats in the shortest round-trip form and **always with a `.0` or exponent** (`1.0`, not `1`).
- `dump` rejects functions, modules, `nan` and infinity.
- Map keys are always strings in both directions.

```my
import json

doc = json.parse("{\"name\": \"ada\", \"langs\": [\"en\", \"fr\"], \"age\": 36, \"score\": 9.5, \"admin\": false, \"boss\": null}")
print(doc["name"])
print(doc["langs"][1])
print(doc["age"] + 1)
print(type(doc["score"]))
print(doc["boss"])
print(json.dump(doc))
```

```output
ada
fr
37
float
None
{"name":"ada","langs":["en","fr"],"age":36,"score":9.5,"admin":false,"boss":null}
```

Pretty printing and round trips:

```my
import json

data = {"items": [1, 2.0, "x\ny"], "empty": [], "nested": {"ok": True}}
print(json.dump(data, 2))
print(json.parse(json.dump(data)) == data)
```

```output
{
  "items": [
    1,
    2.0,
    "x\ny"
  ],
  "empty": [],
  "nested": {
    "ok": true
  }
}
True
```

Malformed input is fatal; validate untrusted text in a child process if you must survive it, e.g. `proc.run(["python3", "-m", "json.tool"], text)["code"] == 0`.

```my-error
import json
json.parse("{\"a\": ")
```

```output
invalid JSON: unexpected end
```

Reading and writing a config file:

```my
import json
import fs

fs.write("/tmp/mplangp_doc_cfg.json", json.dump({"retries": 3}, 2))
cfg = json.parse(fs.read("/tmp/mplangp_doc_cfg.json"))
cfg["retries"] = cfg["retries"] + 1
fs.write("/tmp/mplangp_doc_cfg.json", json.dump(cfg, 2) + "\n")
print(json.parse(fs.read("/tmp/mplangp_doc_cfg.json"))["retries"])
fs.remove("/tmp/mplangp_doc_cfg.json")
```

```output
4
```
