# Module index

Modules are loaded with `import name` and used as `name.function(...)`. Each page lists every function
with argument types, return values, error behaviour and tested examples.

## Choosing a module

| I want to... | use |
|---|---|
| read, write, list, copy, delete files and folders | [fs](fs.md) (+ [path](path.md) for building paths) |
| run a command-line program and get its output | [proc](proc.md) |
| read environment variables, find the OS, exit with a status | [os](os.md) |
| know the CPU count, load, memory, disk space | [sys](sys.md) |
| get the time, wait, format or parse dates | [time](time.md) |
| read user input, write to stderr, use colours | [io](io.md) |
| fetch a web page or call a REST API | [http](http.md) (+ [json](json.md), [url](url.md)) |
| open a raw TCP connection, run a server, resolve DNS, test a port | [net](net.md) |
| parse or produce JSON | [json](json.md) |
| parse or produce CSV / TSV | [csv](csv.md) |
| store and query structured data | [db](db.md) (SQLite) |
| search, extract, replace text with patterns | [re](re.md) |
| split, join, trim, replace plain text | [str](str.md) |
| base64 / hex / percent-encode | [enc](enc.md) |
| checksums, file hashes, HMAC signatures | [hash](hash.md) |
| random numbers, shuffling, ids, tokens | [rand](rand.md) |
| square roots, rounding, min/max, trigonometry | [math](math.md) |
| clipboard, desktop notifications, open a file/URL | [desktop](desktop.md) |

## All functions at a glance

| module | functions |
|---|---|
| [fs](fs.md) | `read write append exists isfile isdir stat list glob mkdir remove rename copy cwd chdir` |
| [path](path.md) | `join base dir ext stem norm abs home` |
| [os](os.md) | `env setenv unsetenv environ platform arch hostname user pid exit` |
| [proc](proc.md) | `run sh which` |
| [sys](sys.md) | `cpus load uptime memory disk uname` |
| [time](time.md) | `now mono sleep format iso parse parts` |
| [io](io.md) | `input readall write eprint flush isatty size color` |
| [net](net.md) | `connect open listen port accept peer send recv readline timeout close resolve` |
| [http](http.md) | `get post request download timeout` |
| [url](url.md) | `parse query` |
| [json](json.md) | `parse dump` |
| [csv](csv.md) | `parse dump` |
| [db](db.md) | `open close exec query last_id` |
| [re](re.md) | `test find findall sub split` |
| [str](str.md) | `len upper lower trim split lines join replace find starts ends fixed pad_left pad_right` |
| [enc](enc.md) | `b64 unb64 hex unhex url unurl` |
| [hash](hash.md) | `sha256 file hmac crc32` |
| [rand](rand.md) | `seed int float choice shuffle hex uuid` |
| [math](math.md) | `sqrt pow floor ceil round abs min max sin cos tan atan2 exp log log10 pi` |
| [desktop](desktop.md) | `clipboard copy notify open` |

## Conventions shared by all modules

- **Errors are fatal.** A bad argument or a failed operation stops the program with `line N: <message>` (exit 70). Messages start with the function and what it expects, e.g. `get(url, headers) takes an http(s) url and optional headers.`
- **Argument checking is strict**: wrong count or wrong types are errors, never silently converted. Numbers accept `int` or `float`; "integer" parameters reject floats (pass `int(x)`).
- **Return values are plain data**: `None`, bool, int, float, str, list or map. Resources (sockets, databases) are integers.
- **Text only**: strings cannot contain NUL bytes; they are dropped from data read from files, sockets, processes and the web, and decoders reject them.
- **Nothing is retained between calls** except explicit handles (sockets, databases), the current directory and the environment.
- Missing optional values are `None` (e.g. `os.env`, `proc.which`, `io.input` at end of input).
