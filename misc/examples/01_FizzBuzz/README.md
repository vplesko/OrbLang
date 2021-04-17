# FizzBuzz

Start with `main` and imports.

```
import "base.orb";
import "clib.orb";

fnc main () () {
};
```

`base.orb` defines numerous useful macros, such as `if`, `for`, `continue`, `&&`, etc.

To iterate from 1 to 100, use `range` macro from `base.orb`.

```
    range i 1 100 {
    };
```

Here, `i` is the name of the variable whose value will iterate from 1 to 100. Its type is implicitly `i32` (32-bit signed integer).

Depending on `i`, print the appropriate string, or just print the value of `i`.

```
        if (== (% i 15) 0) {
            printf "FizzBuzz\n";
            continue;
        };
        if (== (% i 3) 0) {
            printf "Fizz\n";
            continue;
        };
        if (== (% i 5) 0) {
            printf "Buzz\n";
            continue;
        };
        printf "%d\n" i;
```

Orb's syntax is strictly prefix. The first element of a node determines what it represents. For example, `(== (% i 15) 0)` here is checking whether `i` is divisible by 15.

`continue` skips the remainder of the current loop iteration. If no `continue` is executed, `i` gets printed.

## Final code

```
import "base.orb";
import "clib.orb";

fnc main () () {
    range i 1 100 {
        if (== (% i 15) 0) {
            printf "FizzBuzz\n";
            continue;
        };
        if (== (% i 3) 0) {
            printf "Fizz\n";
            continue;
        };
        if (== (% i 5) 0) {
            printf "Buzz\n";
            continue;
        };
        printf "%d\n" i;
    };
};
```