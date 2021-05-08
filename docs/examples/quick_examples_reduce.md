---
layout: default
title: Reduce
---
# {{ page.title }}

This example demonstrates writing macros to make calculations easier. The macros are defined in `reduce.orb`, which then gets imported from `main.orb`.

These macros can iterate over an array (or tuple, data type, `raw`, or string) and perform a cumulative operation on each element.

`genId` is used to make `reduce` hygienic and arguments are marked with `::preprocess` to prevent multiple executions on same nodes.

## `reduce.orb`

```
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
    ret \(reduce ,coll (cast (typeOf ([] ,coll 0)) 0) ,oper);
};
```

## `main.orb`

```
import "base.orb";
import "reduce.orb";
import "std/io.orb";

fnc main () () {
    std.println (reduce (arr i32 10 20 30) +);

    std.println (reduce (arr i32 1 2 3 4) 1 *);

    # polynomial calculation
    std.println (reduce (arr f32 1.0 7.0 12.0 6.0)
        (lam (res:f32 coef:f32) f32 { ret (+ (* res 0.1) coef); }));
};
```