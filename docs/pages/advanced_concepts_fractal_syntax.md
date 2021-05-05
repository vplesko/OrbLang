---
layout: default
title: Fractal syntax
---
# {{ page.title }}

Orb's syntax tends to be more on the open-ended side. For example, call instructions can get their function from another instruction.

```
import "std/io.orb";

fnc main () () {
    (lam (x:i32 y:i32) () { std.println x; std.println y; }) 100 200;
};
```

Special forms can be returned from macros.

```
mac getSym () {
    ret sym;
};

fnc main () () {
    (getSym) x:i32;
};
```

Types can be provided by symbols.

```
fnc main () () {
    eval (sym (arrTy (i32 4)));

    sym a:arrTy;
};
```

They may also be passed from an evaluated block.

```
fnc main () () {
    sym x:(eval (block type {
            pass bool;
        }));
};
```

Operator arguments may be passed from blocks.

```
fnc main () () {
    + (block i32 { pass 1; })
      (block i32 { pass 2; });
};
```

Some arguments in special forms are implictly escaped. To override this, unescape them.

```
fnc main () () {
    eval (sym (myId \foo));

    sym ,myId:i32;
};
```

Feel free to experiment with the syntax!

Overdoing this can result in some very odd-looking code.

```
mac getMainArgs () { ret \(); };

eval (sym (mainRetTy i32));

mac getRet () { ret ret; };

fnc (eval (block id { pass \main; })) ,(getMainArgs) mainRetTy {
    (getRet) (block i32 {
        pass 0;
    });
};
```