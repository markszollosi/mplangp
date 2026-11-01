# rand: random numbers and identifiers

`import rand`. By default values come from the operating system's entropy source
(`/dev/urandom`), which is suitable for tokens and ids. After `seed(n)` the module switches to a fast
**deterministic** generator: the same seed gives the same sequence of numbers, which is what you
want for reproducible tests and simulations, and which is **not secure**. Seeding also makes
`uuid()` and `hex()` predictable, so never seed in code that creates secrets.

| function | returns | notes |
|---|---|---|
| `seed(n)` | None | integer seed; switches to the deterministic generator for the rest of the program |
| `int(lo, hi)` | int | uniform in `lo..hi` **inclusive**; requires `lo <= hi` |
| `float()` | float | uniform in `[0, 1)` |
| `choice(list)` | value | random element; the list must be non-empty and hold only plain values (None, bool, int, float, str) |
| `shuffle(list)` | None | shuffle **in place** |
| `hex(nbytes)` | str | `2 * nbytes` random hex characters, `nbytes` 1 to 4096 (API keys, tokens) |
| `uuid()` | str | random version-4 UUID like `"3f2b8c1e-9a47-4d55-8e0b-5d1c2a7f9b60"` |

```my
import rand

rand.seed(42)
first = [rand.int(1, 100), rand.float()]
rand.seed(42)
second = [rand.int(1, 100), rand.float()]
print(first == second)

deck = range(10)
rand.shuffle(deck)
sort(deck)
print(deck)
print(rand.choice(["only"]))
print(len(rand.uuid()))
print(len(rand.hex(16)))
roll = rand.int(1, 6)
print(roll >= 1 and roll <= 6)
```

```output
True
[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
only
36
32
True
```

There is no `sample` or weighted choice: shuffle a copy and take the first `k` elements, or loop with `rand.float()`.
