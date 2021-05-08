---
layout: default
title: BinaryTree
---
# {{ page.title }}

This example has a more complex macro definition called `makeBinTree`, used to construct a binary tree through more convenient syntax.

Under the hood, a tree is a `std.One` value pointing to a `BinNode`. Using `std.One` makes it easy to allocate nodes, and automatically cleans up the tree after use. This cleans up the entire tree, including all direct and indirect children nodes.

The macro uses various definitions from **base.orb** and **std/One.orb**. `make` is used to initialize `BinNode` and `std.makeOneWith` is used to allocate a copy of that value on the heap and return a `std.One` pointing to it. `base.isOfType` and `base.isEmptyRaw` help analyze user input.

The `main` function uses this macro to build several trees more easily and perform some analysis on them.

## `binTree.orb`

```
import "std/One.orb";

data BinNode {
    val:i32
    left:(std.One BinNode false)
    right:(std.One BinNode false)
};

# couldn't define the drop function earlier due to circular definitions
std.defineDrop (std.One BinNode);

mac makeBinTree (tree) {
    if (base.isOfType tree raw) {
        # expecting three entries for each field of BinNode
        if (!= (lenOf tree) 3) {
            message::error tree::loc "Bad binary tree structure.";
        };

        # create a code snippet for initializing BinNode
        sym (code \(make BinNode (val ,([] tree 0))));

        # recursively construct left and right subtree
        if (! (base.isEmptyRaw ([] tree 1))) {
            = code (+ code \((left (makeBinTree ,([] tree 1)))));
        };
        if (! (base.isEmptyRaw ([] tree 2))) {
            = code (+ code \((right (makeBinTree ,([] tree 2)))));
        };

        # return code that initializes std.One pointing to a BinNode
        ret \(std.makeOneWith ,code);
    };

    # else, it's a leaf node
    ret \(std.makeOneWith (make BinNode (val ,tree)));
};
```

## `main.orb`

```
import "base.orb";
import "std/io.orb";
import "binTree.orb";

# recursively calculates the sum and number of all values in a binary tree
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
                    ()
                    (1
                        0
                        4)))));

    # single node tree
    printSumAndCnt (makeBinTree (3 () ()));

    # single node tree given as a single value
    # this proves the macro works on identifiers
    sym (x 8);
    printSumAndCnt (makeBinTree x);
};
```