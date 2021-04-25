# Architecture

This document briefly lists different parts of this repository and how they interact. It should be a good starting point to understanding the project structure. It is written as an info dump and intentionally kept as short as possible.

## Directories

**/src** contains the source code of the Orb compiler. Both header and source files are in the same directory.

**/libs** contains Orb library files that get installed along with the compiler.

**/tests** contains test cases for the compiler. Positive test cases are in **/tests/positive** and consist of an **.orb** file and a text file of the same name. Each positive test case is expected to compile, and after the compiled program is ran, its output must match the contents of the corresponding text file. Negative test cases are in **/tests/negative** and the compiler is expected to report an error when trying to compile them.

**/docs** contains the files for GitHub pages of this project. They are built using Jekyll.

## Codebase

### Data

`CodeLoc` marks the starting and ending code locations of an element. Ending locaton is one character past the actual ending, except when it isn't.

`NodeVal` is the main type that gets passed around during compilation. It is both a node in ASTs and any possible value that can be created in Orb. It contains all of the info relating to that value (eg. code location, escaping, type). It can contain any of several different objects (some are described below). It can be invalid - make sure to check for this when a function returns it! Since it may point to arbitrarily large amounts of data, it should be copied sparingly.

`EvalVal` is an evaluated value - something that was calculated at compile-time. `LlvmVal` is a compiled value.

`LiteralVal` is a literal value. Orb programmers never get to interact with them, as they quickly get promoted to `EvalVal`s.

`SpecialVal` denotes a special form. `UndecidedCallableVal` denotes a function or macro name where there may be multiple such callables sharing the same name.

`AttrMap` represents a map for non-type attributes.

`Token`s are tiny pieces of syntax generated from input source files.

### State

`NamePool` contains identifiers appearing in the code. They are keyed by `NamePool::Id`, which callers use to interact with this class.

`StringPool` contains string literals appearing in the code. They are keyed by `StringPool::Id`, which callers use to interact with this class.

`TypeTable` contains all types defined in the program. They are keyed by `TypeTable::Id`, which callers use to interact with this class.

`SymbolTable` contains all symbols which are currently defined and maintains all scopes which are currently in effect. It contains all currently declared/defined functions and macros. Additionally, it keeps track of some other things, eg. attributes and drop functions of data types. `BlockRaii` is a utility class for opening and closing new scopes.

`CompilationMessages` is used to print various messages (eg. errors, warnings, user messages) during compilation. It also keeps track of whether compilation has failed.

### Processing

`Lexer` goes through a source file and produces a stream of `Token`s.

`Parser` parses a file, producing `NodeVal`s by grouping `Token`s and nested `NodeVal`s.

`Processor` is an abstract class, which processes `NodeVal`s to create new ones. It performs all necessary error-checking and reporting. It updates the state through above-mentioned state classes.

`Evaluator` is an implementation of `Processor` which deals with compile-time evaluations. `Compiler` is an implementation of `Processor` which compiles values and code. Both of these have pointers to each other and can tell which one they are.

`CompilationOrchestrator` initializes all necessary classes, performs dependency injection, and makes sure initial types and keywords are defined. It coordinates the compilation process by calling into other classes and takes care of file switching when a new file is being imported.

`main()` function initializes `ProgramArgs` from compiler arguments and calls into the compilation process. It returns the proper exit code on error. It should not allow any exceptions defined in the project to fall through. These must be defined in **exceptions.h**.