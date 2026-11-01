# enc: text encodings

`import enc`. Base64, hexadecimal and URL (percent) encoding of strings.

| function | returns | notes |
|---|---|---|
| `b64(text)` | str | standard Base64 with `=` padding |
| `unb64(text)` | str | decode; accepts the URL-safe alphabet (`-` `_`), ignores whitespace and padding |
| `hex(text)` | str | lower-case hex of every byte (`"abc"` gives `"616263"`) |
| `unhex(text)` | str | decode hex (even length, upper or lower case) |
| `url(text)` | str | percent-encode everything except letters, digits and `-_.~` (spaces become `%20`) |
| `unurl(text)` | str | decode `%XX` and turn `+` into a space |

Decoders fail (fatal) on invalid input, and also if the decoded bytes would contain a NUL byte, because strings cannot hold NUL. This means these functions cover **text**, not arbitrary binary files. For binary data use the command line: `proc.run(["base64", file])["out"]`, `proc.run(["xxd", file])`.

```my
import enc

print(enc.b64("Man"))
print(enc.b64("Ma"))
print(enc.unb64("TWFuIGlzIGRpc3Rpbmd1aXNoZWQ="))
print(enc.hex("abc"))
print(enc.unhex("616263"))
print(enc.url("a b&c=d/é"))
print(enc.unurl("a%20b%26c+d"))
```

```output
TWFu
TWE=
Man is distinguished
616263
abc
a%20b%26c%3Dd%2F%C3%A9
a b&c d
```

HTTP Basic authentication header:

```my
import enc

print("Basic " + enc.b64("user:password"))
```

```output
Basic dXNlcjpwYXNzd29yZA==
```

To build a whole query string from a map use `url.query` ([url.md](url.md)); `enc.url` is for single values.
