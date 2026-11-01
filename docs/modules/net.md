# net: TCP sockets and DNS

`import net`. Low-level networking: open connections, run a server, look up names. For web
requests use [http](http.md) instead. A socket is an **integer handle**; pass it to the other
functions and `close` it when done (handles are closed automatically when the program exits).

| function | returns | notes |
|---|---|---|
| `connect(host, port, timeout?)` | int | open a TCP connection. Default timeout 10 seconds. Fails (fatal) if the host does not resolve, refuses or times out |
| `open(host, port, timeout?)` | bool | whether a TCP connection to `host:port` can be made (default timeout 2 s). The probe closes itself; never fatal for refused/unreachable |
| `listen(port, host?)` | int | start listening. `port` `0` picks a free port; `host` defaults to `"127.0.0.1"` (this computer only). Use `"0.0.0.0"` to accept connections from the network |
| `port(fd)` | int | local port of a socket (use after `listen(0)`) |
| `accept(fd)` | int | wait for a client; returns a new socket for it |
| `peer(fd)` | str | IP address of the other side |
| `send(fd, text)` | int | send **all** of `text`; returns the byte count |
| `recv(fd, max)` | str | wait for data, return up to `max` bytes (1 to 1048576), whatever has arrived; **`""` means the other side closed** |
| `readline(fd)` | str or None | read up to a newline and strip `\r\n`; `None` at end of stream |
| `timeout(fd, seconds)` | None | make `recv`/`readline`/`send` give up after this long (`0` = wait forever, the default). Giving up is a fatal `timed out` error |
| `close(fd)` | None | |
| `resolve(host)` | list of str | IPv4 and IPv6 addresses for a name, deduplicated |

Strings cannot carry NUL bytes, so this is for text protocols (HTTP, SMTP, Redis, line protocols, JSON over TCP). A server handles **one connection at a time** (accept, serve, close, repeat).

## Loopback demo (client and server in one program)

A connection to a listening socket completes before `accept`, so one program can play both sides for small messages:

```my
import net

server = net.listen(0)
port = net.port(server)
client = net.connect("127.0.0.1", port)
conn = net.accept(server)

net.send(client, "hello server\r\n")
print(net.readline(conn))
net.send(conn, "pong")
net.close(conn)
print(net.recv(client, 100))
print(net.recv(client, 100) == "")
print(net.peer(client))

net.close(client)
net.close(server)
print(net.open("127.0.0.1", port, 1))
print(net.resolve("127.0.0.1"))
```

```output
hello server
pong
True
127.0.0.1
False
["127.0.0.1"]
```

## Patterns

A small server that answers each client with a line:

```my-norun
import net

server = net.listen(8080)
print("listening on 8080")
while True:
    conn = net.accept(server)
    net.timeout(conn, 5)
    request = net.readline(conn)
    if request != None:
        net.send(conn, "echo: " + request + "\n")
    net.close(conn)
```

Talking raw HTTP/1.0 (read until the server closes):

```my-norun
import net

s = net.connect("example.com", 80)
net.send(s, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
reply = ""
while True:
    chunk = net.recv(s, 4096)
    if chunk == "":
        break
    reply = reply + chunk
net.close(s)
print(slice(reply, 0, 15))
```

Checking whether a service is up:

```my-norun
import net
print(net.open("localhost", 5432))
```

TLS is not available through `net`; use `http` for `https://` or run a tool such as `openssl s_client` through `proc`.
