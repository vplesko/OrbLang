---
layout: default
title: BinaryTree
---
# {{ page.title }}

## `main.orb`

```
import "base.orb";
import "std/io.orb";
import "std/One.orb";

data BinNode {
    val:i32
    left:(std.One BinNode false)
    right:(std.One BinNode false)
};

std.defineDrop (std.One BinNode);

mac makeBinTree (tree) {
    if (|| (base.isOfType tree i32) (base.isOfType tree id)) {
        ret \(std.makeOneWith (make BinNode (val ,tree)));
    } (base.isOfType tree raw) {
        if (!= (lenOf tree) 3) {
            message::error tree::loc "Bad binary tree structure.";
        };

        sym (code \(make BinNode (val ,([] tree 0))));

        if (! (base.isEmptyRaw ([] tree 1))) {
            = code (+ code \((left (makeBinTree ,([] tree 1)))));
        };
        if (! (base.isEmptyRaw ([] tree 2))) {
            = code (+ code \((right (makeBinTree ,([] tree 2)))));
        };

        ret \(std.makeOneWith ,code);
    };

    message::error tree::loc "Invalid type '"
        (typeOf tree) "' for binary tree structure.";
};

fnc sumAndCnt (node:(BinNode cn *)) (i32 i32) {
    if (== node null) {
        ret (tup 0 0);
    };

    sym (leftRes (sumAndCnt (std.getPtr (-> node left))))
        (rightRes (sumAndCnt (std.getPtr (-> node right))));

    ret (tup (+ ([] leftRes 0) ([] rightRes 0) (-> node val))
        (+ ([] leftRes 1) ([] rightRes 1) 1));
};

fnc printSumAndCnt (tree:(std.One BinNode)::noDrop) () {
    sym (result (sumAndCnt (std.getPtr tree)));
    std.println ([] result 0) ' ' ([] result 1);
};

fnc main () () {
    printSumAndCnt (makeBinTree
        (8
            (4
                2
                1)
            (3
                1
                0)));

    printSumAndCnt (makeBinTree
        (2
            (3
                1
                ())
            (-1
                0
                (5
                    4
                    (1
                        0
                        ())))));

    printSumAndCnt (makeBinTree (3 () ()));

    sym (x 8);
    printSumAndCnt (makeBinTree x);
};
```