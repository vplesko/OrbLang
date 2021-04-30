---
layout: default
title: Functions
---
# {{ page.title }}

Functions are reusable pieces of code that can be called. They are defined with the `fnc` special form.

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

Functions can be declared by ommiting the function body.

```
fnc square (x:f64) f64;
```