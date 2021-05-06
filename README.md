# Orb

Orb is a general-purpose programming language. It is procedural, statically typed, and compiled, with scope-based memory management. A major feature are its powerful meta-programming capabilities - you can use identifiers, types, and even Orb code itself as first-class values at compile-time.

It offers a powerful macro system - procedures that return Orb code to be compiled. This allows you to parameterize and reuse similar bits of code, making your codebase more concise and easily readable.

> This project is a hobby effort and not mature enough for anything beyond writing toy programs.

To learn more about the language, please read [the Orb documentation](https://vplesko.github.io/OrbLang/).

## Installing

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

## Examples

**Visit [the quick examples sections](https://vplesko.github.io/OrbLang/examples/quick_examples.html) of the Orb documentation to see what Orb code looks like.**

```
import "base.orb";
import "std/io.orb";

mac reduce (coll::preprocess start::preprocess oper) {
    sym (acc (genId)) (iter (genId));

    ret \(block ,(typeOf start) {
        sym (,acc ,start);
        range ,iter ,(lenOf coll) {
            = ,acc (,oper ,acc ([] ,coll ,iter));
        };
        pass ,acc;
    });
};

mac reduce (coll::preprocess oper) {
    ret \(reduce ,coll 0 ,oper);
};

fnc main () () {
    std.println (reduce (arr i32 10 20 30) +);

    std.println (reduce (arr i32 1 2 3 4) 1 *);
};
```

`libs/` contains source files for standard use in Orb projects. Reading these will give you more ideas on what is possible with Orb.

Additionally, you may also browse `.orb` files in `tests/positive/`. These are test files for the project. They may not be the best examples of a good code style, however.