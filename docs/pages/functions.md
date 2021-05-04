---
layout: default
title: Functions
---
# {{ page.title }}

Functions are subprocedures that can be called. They are defined with the `fnc` special form.

```
fnc sayHello () () {
    std.println "Hello!";
};
```

Functions can return values by using the `ret` keyword. This function takes a single argument of type `i32` and returns a value of the same type:

```
fnc square (x:i32) i32 {
    ret (* x x);
};
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

Overloaded functions must not conflict with each other, meaning they have to have different signatures. This definition would raise errors:

```
fnc square (x:i32) i64 {
    ret (* (cast i64 x) x);
};
```
{: .code-error}

Functions can be declared by ommiting the function body. A declared function may be defined later in the code.

```
fnc square (x:f64) f64;
```

> There is a convention to name your functions and macros as `MODULE_NAME.FUNCTION_NAME`. If the function or macro is not intended to be called directly by your users, its name should be prepended with a minus: `MODULE_NAME.-FUNCTION_NAME`. A lot of functions and macros from **base.orb** don't follow this convention for the sake of user convenience.