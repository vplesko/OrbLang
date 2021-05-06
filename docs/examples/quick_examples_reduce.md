---
layout: default
title: Reduce
---
# {{ page.title }}

## `main.orb`

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
    ret \(reduce ,coll ([] ,coll 0) ,oper);
};

fnc main () () {
    std.println (reduce (arr i32 10 20 30) 0 +);

    std.println (reduce (arr i32 1 2 3 4) *);
};
```