# Orb

Orb is a general-purpose compiled programming language. It features static typing, manual memory management, and compile-time computations. Identifiers, types, and even Orb code itself are all first-class values during compilation of Orb programs. Orb's powerful macro system allows you to freely extend the language syntax and offers good code reuse potential.

> This project is a hobby effort and very much a work in progress. A number of features are still missing and the tooling may not be very user-friendly.

## Examples

**If you are interested in what Orb code looks like, look inside `doc/examples/` for some examples with explanations.**

`libs/` contains source files for standard use in Orb projects. Reading these will give you more ideas on what is possible with Orb.

Additionally, you may also browse `.orb` files in `tests/positive/`. These are test files for the project.

## Install

The following are required:
 - Cmake version 3.4.3 or greater
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