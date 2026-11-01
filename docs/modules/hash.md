# hash: digests and checksums

`import hash`. All digests are returned as lower-case hexadecimal strings.

| function | returns | notes |
|---|---|---|
| `sha256(text)` | str (64 hex chars) | SHA-256 of the text's bytes |
| `file(path)` | str (64 hex chars) | SHA-256 of a file's contents; works for **binary** files of any size (streamed) |
| `hmac(key, text)` | str (64 hex chars) | HMAC-SHA256 |
| `crc32(text)` | int | CRC-32 (IEEE) checksum, `0` to `4294967295` |

MD5 and SHA-1 are not provided; if a legacy system demands them, run `md5`/`shasum` through `proc`.

```my
import hash

print(hash.sha256("abc"))
print(hash.hmac("key", "The quick brown fox jumps over the lazy dog"))
print(hash.crc32("123456789"))
```

```output
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8
3421780262
```

Verify a downloaded file:

```my
import hash
import fs

fs.write("/tmp/mplangp_doc_hash.txt", "abc")
print(hash.file("/tmp/mplangp_doc_hash.txt") == hash.sha256("abc"))
fs.remove("/tmp/mplangp_doc_hash.txt")
```

```output
True
```

Signing a webhook payload: `signature = hash.hmac(secret, body)` and compare with the header value. For storing passwords use a real password hash tool (`proc.run(["htpasswd", ...])`, `argon2`); a bare SHA-256 is not suitable.
