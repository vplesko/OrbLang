---
layout: default
title: Macros
---
# {{ page.title }}

Macros are subprocedures, in some ways similar to functions, that take a number of arguments and return a value. Both the arguments and the return value may be anything representable through Orb syntax.

After the invocation, the returned value is processed by the compiler.

> Functions are said to be called, while macros are invoked.

Here is an example of a simple macro and its usage:

```
import "base.orb";
import "std/io.orb";

mac doThrice (body) {
    ret \(repeat 3 ,body);
};

fnc main () () {
    doThrice {
        std.println "In triplicate.";
    };
};
```

Before the invocation, macro arguments are first escaped, then processed. As a consequence, the value we are passing to `doThrice` is of type `raw`. The macro returns another `raw` value, which is a snippet of code.

This snippet of code is now processed. It happens to be an invocation of the `repeat` macro, so that macro is invoked next.

Notice how we constructed the return value inside `doThrice`. This is a common pattern, where the entire value is escaped, but then all references to macro arguments are unescaped.

Just as is the case for elements of `raw` values, macro arguments (and return values, for that matter) retain their non-value node properties.

Neither macro arguments nor their return values need to be of type `raw`.

```
import "std/io.orb";

mac printThrice (x delim) {
    ret \(std.println ,x ,delim ,x ,delim ,x);
};

fnc main () () {
    printThrice "triplicate" ' ';
};
```

(**Warning**: The macro above contains a subtle error! Continue reading to learn what it is.)

Macro invocations are always evaluated, though their arguments may be compiled values. This works, since usually macros simply pack their arguments in a code snippet and return it, without directly interacting with those values.

Just like functions, macros have types. For example, `(mac 1)` expresses the type of macros that take a single argument. You may have symbols of macro types and even pass them as arguments to functions. Whether or not you should do this is up to you. If you do, you may find `pat` macros (patterns) useful - patterns are to macros what lambdas are to functions.

**Macros are dangerous!** If you are not careful, your code may result in errors that will be very hard to debug. When writing macros, keep the following dangers in mind:

 - **No guarantees on argument types**
 - **Unhygienic symbol declarations**
 - **Potential multiple executions**
 - **Drop unsafety**

The first one should be obvious. There is no way to specify what the argument types are, so if your macro code relies on that, you need to manually perform the checks.

Our `printThrice` macro suffers from the third danger, potential multiple execution.

```
fnc main () () {
    sym (x "danger!");

    printThrice (>> x) ' ';
};
```
{: .code-error}

This code has undefined behaviour. Why? In the code returned from `printThrice`, the first argument to `std.println` would return the value of `x`, but then also reset it to `null`. The next time we attempt to print `x`, it will be `null`, and `std.println` cannot handle that.

There is actually a rather short and easy fix to this problem. Read the following sections to find out how to protect against these dangers.