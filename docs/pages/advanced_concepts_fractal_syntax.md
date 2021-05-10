---
layout: default
title: Fractal syntax
---
# {{ page.title }}

Orb's syntax tends to be more on the open-ended side. We saw that each node is processed by first processing its starting node, then operating on the arguments depending on what the starting node represented. Very often, these child nodes can be nested code that returns something.

For example, we can define a function in a call instruction and then immediately call it.

```
import "std/io.orb";

fnc main () () {
    (lam (x:i32 y:i32) () { std.println (+ x y); }) 100 200;
};
```

You may even return special forms from a macro and use them as the starting node, should you ever need to.

```
mac getSym () {
    ret sym;
};

fnc main () () {
    (getSym) x:i32;
};
```

Arguments can almost always be returned from a call, invocation, or block.

```
fnc main () () {
    + (block i32 { pass 1; })
      (block i32 { pass 2; });
};
```

This even applies to types (remember that types can only be used in evaluated code).

```
fnc main () () {
    sym a:(eval (block type {
        pass i32;
    }));
};
```

It also applies to the names of new symbols, functions, macros, and types.

```
eval (fnc makeName (baseName:id ty:type) id {
    ret (+ baseName (cast id ty));
});

eval (sym (ty i32));

data (makeName \foo ty) {
    x:ty
};
```

However, if you want to load the name from another symbol, you must escape it first.

```
fnc main () () {
    eval (sym (myId \foo));

    sym ,myId:i32;
};
```

Feel free to experiment with the syntax!

Be warned that overdoing this can result in some very odd-looking code.

```
import "base.orb";

eval (sym (mainName \main));

mac getMainArgs () { ret (); };

eval (sym (mainRetTy i32));

mac getRet () { ret ret; };

fnc ,mainName ,(getMainArgs) mainRetTy {
    (getRet) (block i32 {
        pass 0;
    });
};
```