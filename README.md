# Orb

Orb is a general-purpose compiled programming language. It features strict typing, compile-time computations, and manual memory management.

**This project is a hobby effort and very much a work in progress. A good number of key features are still missing and the tooling may not be very user-friendly.**

> Check `doc/examples` for some examples for Orb code.

## Install

Following are required:
 - Cmake version 3.4.3 or greater
 - LLVM version 11
 - Clang version 11
 - Python 3 (for testing, optional)

From the top directory of the project, execute the following:

```
mkdir build
cd build
cmake ..
cmake --build .
cd ..
```

The executable `orbc` should appear somewhere within directory `build`, depending on your platform and Cmake configuration.

You may run the tests with:

```
cd tests
python3 run_tests.py <PATH TO ORBC>
cd ..
```

## Examples

You may look inside directory `doc/examples` for examples of code. Additionally, you may also browse `.orb` files in `tests/positive`.

Here is what HelloWorld could look like in Orb:

```
(import "clib.orb")

(fnc main () i32 (
    (printf "Hello, world!\n")
    (ret 0)
))
```

Depending on your preferences, you may instead write it like this:

```
import "clib.orb";

fnc main () () {
    printf "Hello, world!\n";
};
```

This may be compiled to an executable with:

```
./orbc main.orb main
```

Here is an implementation of Fizzbuzz:

```
import "clib.orb";

fnc fizzbuzz (top:u32) () {
    sym (i:u32 1);
    block for-loop () {
        # exit after top is exceeded
        exit (> i top);

        block for-body () {
            block if () {
                # negated if condition
                exit (!= (% i 15) 0);
                printf "FizzBuzz\n";
                # continue to next iteration
                exit for-body true;
            };
            block if () {
                exit (!= (% i 3) 0);
                printf "Fizz\n";
                exit for-body true;
            };
            block if () {
                exit (!= (% i 5) 0);
                printf "Buzz\n";
                exit for-body true;
            };
            printf "%d\n" i;
        };

        # increment the counter
        = i (+ i 1);
        # unconditionally jump to start of block
        loop true;
    };
};

fnc main () () {
    # call FizzBuzz
    fizzbuzz 100;
};
```

Orb has no knowledge of ifs and for loops, instead providing more primitive instructions. However, such constructs can be defined from within Orb source code. Check `doc/examples/base.orb` for a taste of how this works.

Here is an illustration of Orb's type system:

```
bool - boolean
i8, i16, i32, i64 - signed integrals
u8, u16, u32, u64 - unsigned integrals
f32, f64 - floating point numerals
c8 - 8-bit char
ptr - non-descript pointer (void* in C)
id - identifier (first-class value during compile-time)
type - type of a value (first-class value during compile-time)
raw - unprocessed list of values, usually represents Orb code (first-class value during compile-time)
i32 cn - constant i32
i32 4 - array of 4 i32s
i32 * - pointer to a i32 (can be dereferenced)
i32 [] - array pointer to a i32 (can be indexed, but not dereferenced)
i32 cn * - pointer to a constant i32
i32 * cn - constant pointer to a non-constant i32
i32 cn * cn - constant pointer to a constant i32
(i32 i32) - tuple of two i32s
(i32 (u32 cn *) (c8 bool)) - tuple of a i32, a pointer to a constant u32, and a tuple of a c8 and bool
```

`eval` is used to execute code at compile-time. This code calculates the 20th number in the Fibonacci sequence while compiling, and prints it at runtime:

```
import "clib.orb";

# define our own type (just for show)
eval (sym (Int:(type cn) i64));

fnc main () () {
    printf "%lld\n" (eval (block Int {
        sym (a:(Int 3));
        = ([] a 0) ([] a 1) 1;

        sym (ind:Int 2) (wanted:Int 20);
        block {
            = ([] a 2) (+ ([] a 0) ([] a 1));
            = ([] a 0) ([] a 1);
            = ([] a 1) ([] a 2);
            = ind (+ ind 1);
            loop (< ind wanted);
        };

        # return a value from this block
        pass ([] a 2);
    }));
};
```