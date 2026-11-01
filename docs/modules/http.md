# http: web requests

`import http`. HTTP and HTTPS client (libcurl): fetch pages, call JSON APIs, download files.
Redirects are followed (up to 10), TLS certificates are verified, system proxy settings
(`https_proxy` etc.) are honoured, and the `User-Agent` is `mplangp`.

| function | returns | notes |
|---|---|---|
| `get(url, headers?)` | map | GET |
| `post(url, body, headers?)` | map | POST with a string body |
| `request(method, url, body?, headers?)` | map | any method; `method` must be upper case (`"PUT"`, `"DELETE"`, `"PATCH"`, `"HEAD"`); `body` string or `None` |
| `download(url, path)` | int | stream a GET response into a file; returns the HTTP status (the file is written whatever the status) |
| `timeout(seconds)` | None | total time allowed per request from now on; default 30, range above 0 to 3600 |

`url` must start with `http://` or `https://`. `headers` is a map of string to string (or `None`).

**Response map:**

| key | value |
|---|---|
| `"status"` | int HTTP status code |
| `"ok"` | `True` for 2xx |
| `"body"` | str (NUL bytes dropped, at most 64 MB) |
| `"headers"` | map of lower-case header name to value (a repeated header keeps its last value) |

## Errors

- An HTTP error status (404, 500...) is **not** an error: check `r["ok"]` or `r["status"]`.
- Transport problems (DNS failure, refused connection, TLS failure, timeout) are **fatal**: `request failed: Failed to connect to 127.0.0.1 port 1 ...`. If the program must survive an unreachable server, probe first with `net.open(host, port)` or run the request in a child: `proc.run(["curl", "-sS", "-m", "10", url])` returns the failure as data.
- A body sent with `post`/`request` defaults to `Content-Type: application/x-www-form-urlencoded`; set the header yourself for JSON or text.

```my-error
import http
http.get("ftp://example.com/file")
```

```output
get(url, headers) takes an http(s) url and optional headers.
```

## Examples

These need a network and are shown, not executed:

```my-norun
import http
import json

r = http.get("https://api.github.com/repos/torvalds/linux", {"Accept": "application/vnd.github+json"})
if r["ok"]:
    repo = json.parse(r["body"])
    print(repo["full_name"] + " has " + text(repo["stargazers_count"]) + " stars")
else:
    print("HTTP " + text(r["status"]))
```

JSON API with an auth header:

```my-norun
import http
import json

payload = json.dump({"title": "hello", "tags": ["a", "b"]})
r = http.post("https://example.com/api/items", payload, {
    "Content-Type": "application/json",
    "Authorization": "Bearer " + token,
})
print(r["status"])
print(r["headers"]["content-type"])
```

Form post, other methods, downloads:

```my-norun
import http
import url

body = url.query({"user": "ada", "q": "a b"})
http.post("https://example.com/form", body)
http.request("DELETE", "https://example.com/api/items/7")
status = http.download("https://example.com/big.zip", "/tmp/big.zip")
```

Query strings: build them with `url.query(map)` so values are percent-encoded.

Test a client without the internet by serving files locally: `proc.sh("python3 -m http.server 8000 &")` in another shell, then `http.get("http://127.0.0.1:8000/")`.
