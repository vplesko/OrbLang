# HelloWorld

Start by defining a minimal main function (entry point of the program).

```
(fnc main () () ())
```

This defines a function called `main` that takes no arguments (first `()`), returns no values (second `()`), and does nothing in its body (third `()`).

Import `clib.orb`, which contains declarations of a few useful C functions.

```
(import "clib.orb")
```

Call `printf` from inside main.

```
    (printf "Hello, world!\n")
```

The syntax is not limited to just parenthesis. `;` can be used to terminate instructions, and `{}` is interchangeable with `()`.

```
fnc main () () {
    printf "Hello, world!\n";
};
```

## Final code

```
import "clib.orb";

fnc main () () {
    printf "Hello, world!\n";
};
```