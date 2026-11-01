i = 1
while i <= 15:
    if i % 15 == 0:
        print("FizzBuzz")
    elif i % 3 == 0:
        print("Fizz")
    elif i % 5 == 0:
        print("Buzz")
    else:
        print(i)
    i = i + 1
n = 0
while n < 6:
    if n % 2 == 0 and not n == 4 or n == 5:
        print(n)
    n = n + 1
print(not True)
print(True and False)
print(False or True)
