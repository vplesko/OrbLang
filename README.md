# Orb

Orb is a general-purpose programming language. It is procedural, statically typed, and compiled, with scope-based memory management. A major feature are its powerful meta-programming capabilities - you can use identifiers, types, and even Orb code itself as first-class values at compile-time.

It offers a powerful macro system - procedures that return Orb code to be compiled. This allows you to parameterize and reuse similar bits of code, making your codebase more concise and easily readable.

> This project is a hobby effort and not mature enough for anything beyond writing toy programs.

To learn more about the language, please read [the Orb documentation](https://vplesko.github.io/OrbLang/).

## Examples

**Visit [the quick examples sections](https://vplesko.github.io/OrbLang/examples/quick_examples.html) of the Orb documentation to see what Orb code looks like.**

```
import "std/io.orb";

eval (sym (n 100));

fnc main () () {
    std.print "Primes:";

    sym arr:(bool (+ n 1));
    range i 2 n {
        if ([] arr i) {
            continue;
        };

        std.print ' ' i;

        range j (* i i) n i {
            = ([] arr j) true;
        };
    };

    std.println;
};
```

`libs/` contains source files for standard use in Orb projects. Reading these will give you more ideas on what is possible with Orb.

Additionally, you may also browse `.orb` files in `tests/positive/`. These are test files for the project.

## Install

The following are required:
 - CMake version 3.4.3 or greater
 - LLVM version 12
 - Clang version 12
 - Python 3 (for testing, optional)

From the top directory of the project, execute the following:

```
mkdir build
cd build
cmake ..
make install
```

Optionally, you can also run the tests with:

```
cd tests
python3 run_tests.py orbc
```