---
layout: default
title: Functions
---
# {{ page.title }}

Functions are subprocedures that can be called multiple times. They are defined with the `fnc` special form.

```
fnc sayHello () () {
    std.println "Hello!";
};
```

This function can then be called with an instruction starting with that function's name. No additional parenthesis are required.

```
fnc main () () {
    sayHello;
};
```

Functions can return values by using the `ret` keyword. The `square` function below takes a single argument of type `i32` and returns a value of the same type:

```
fnc square (x:i32) i32 {
    ret (* x x);
};
```

To call this function we need to provide an argument.

```
    std.println (square 13);
```

Functions can be overloaded by other functions of the same name.

```
fnc square (x:u32) u32 {
    ret (* x x);
};

fnc square (x:f32) f32 {
    ret (* x x);
};
```

Overloaded functions must not conflict with each other, meaning they need to have different signatures. This definition would now raise errors due to conflicts:

```
fnc square (x:i32) i64 {
    ret (* x x);
};
```
{: .code-error}

Functions can be declared by omitting the function body. A declared function may be defined later in the code.

```
fnc square (x:f64) f64;
```

> If you are writing a libary, there is a convention to name your functions and macros as `LIBRARY_NAME.FUNCTION_NAME`. If the function or macro is not intended to be called directly by your users, its name should be prepended with a minus: `LIBRARY_NAME.-FUNCTION_NAME`. A lot of functions and macros from **base.orb** don't follow this convention for the sake of user convenience.