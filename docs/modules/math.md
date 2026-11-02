# math: numbers

`import math`. Arguments may be `int`, `float` or `bool`. Domain errors (square root of a
negative number, logarithm of zero or less) are fatal runtime errors, not `nan`.

| function | returns | notes |
|---|---|---|
| `sqrt(x)` | float | error if `x < 0` |
| `pow(x, y)` | float | `x` to the power `y` |
| `floor(x)` / `ceil(x)` | int | round down / up |
| `round(x)` | int | nearest integer, halves away from zero (`round(2.5)` is `3`, `round(-2.5)` is `-3`) |
| `abs(x)` | same type as `x` | `abs` of the smallest int (-9223372036854775808) wraps and returns it unchanged |
| `min(a, b, ...)` / `max(a, b, ...)` | int or float | one or more numbers; keeps the type of the chosen value |
| `sin(x)` `cos(x)` `tan(x)` | float | radians |
| `atan2(y, x)` | float | angle in radians |
| `exp(x)` | float | e to the power `x` |
| `log(x)` / `log10(x)` | float | natural / base-10 logarithm; error if `x <= 0` |
| `pi()` | float | 3.14159... (a function, there are no module constants) |

There is no `//` or `**` operator: use `math.floor(a / b)` and `math.pow(a, b)`. Integer division
that must stay exact for huge values: `(a - a % b) / b` is a float, so prefer `int(a / b)` when `a`
is below 2^53.

```my
import math

print(math.sqrt(16))
print(math.pow(2, 10))
print(math.floor(7.9))
print(math.ceil(7.1))
print(math.round(2.5))
print(math.round(-2.5))
print(math.abs(-3))
print(math.abs(-3.5))
print(math.min(4, 2, 9))
print(math.max(1, 2.5))
print(math.floor(math.log10(12345)) + 1)
print(math.round(math.atan2(1, 1) * 4 * 1000000) == math.round(math.pi() * 1000000))
```

```output
4.0
1024.0
7
8
3
-3
3
3.5
2
2.5
5
True
```

```my-error
import math
math.sqrt(-1)
```

```output
sqrt of a negative number.
```

Clamping and averages:

```my
import math

def clamp(x, lo, hi):
    return math.max(lo, math.min(x, hi))

total = 0
nums = [3, 5, 10]
for n in nums:
    total = total + n
print(clamp(15, 0, 10))
print(total / len(nums))
```

```output
10
6.0
```
