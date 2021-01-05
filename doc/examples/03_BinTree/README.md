# BinTree

Let `Node` represent a node in a binary tree. It will have an `i32` value and pointers to left and right child nodes.

```
data Node {
    val:i32
    left:(Node *)
    right:(Node *)
};
```

`data` is used to define data types - collections of named values. `Node *` is a type - a pointer to `Node`.

While we're at it, let's define a utility function for constructing a `Node`. Members of data types are accessed with `.` operator.

```
fnc makeNode (val:i32 left:(Node *) right:(Node *)) Node {
    sym node:Node;
    = (. node val) val;
    = (. node left) left;
    = (. node right) right;
    ret node;
};
```

Our goal is to define `sumAndCnt`, a function that calculates the sum and number of elements in a binary tree.

```
fnc sumAndCnt (node:(Node cn *)) (i32 i32) {
```

`Node cn *` is a pointer to a constant `Node`. The value it points to cannot be modified. Note that the pointer itself is not constant, though we could have declared it as such with `Node cn * cn`.

`(i32 i32)` is a tuple. In this case, it is a collection of two `i32` values. Tuple members are also accessed using `.`, but using numerical indexes, instead of identifiers.

```
    if (== node null) {
        ret (tup 0 0);
    };

    sym (leftRes (sumAndCnt (-> node left)))
        (rightRes (sumAndCnt (-> node right)));
```

Here we cover the base case, and the recursive calls. `tup` constructs a tuple out of given values. `->` is a macro for accessing members behind a pointer.

```
    ret (tup (+ (. leftRes 0) (. rightRes 0) (-> node val))
        (+ (. leftRes 1) (. rightRes 1) 1));
```

We return a pair of values, first being the sum of all elements in this subtree, second being their count. `+`, like many other things in Orb, can handle multiple arguments. In this case, both `+`s perform addition on 3 values.

To call `sumAndCnt` from `main`, we need to construct a binary tree. We will construct this one:

```
            2
    3               -1
1       X       0       5
```

, by using `makeNode`.

```
    sym (ll (makeNode 1 null null));
    sym (l (makeNode 3 (& ll) null));
    sym (rl (makeNode 0 null null)) (rr (makeNode 5 null null));
    sym (r (makeNode -1 (& rl) (& rr)));
    sym (root (makeNode 2 (& l) (& r)));
```

`&` is used to get the pointer to a value. Alternatively, we could have used `malloc` to store these nodes on heap, but we would have to `free` them later.

Now, we call `sumAndCnt` and print the result.

```
    sym (result (sumAndCnt (& root)));

    printf "%d %d\n" (. result 0) (. result 1);
```

# Final code

```
import "base.orb";
import "clib.orb";

data Node {
    val:i32
    left:(Node *)
    right:(Node *)
};

fnc makeNode (val:i32 left:(Node *) right:(Node *)) Node {
    sym node:Node;
    = (. node val) val;
    = (. node left) left;
    = (. node right) right;
    ret node;
};

fnc sumAndCnt (node:(Node cn *)) (i32 i32) {
    if (== node null) {
        ret (tup 0 0);
    };

    sym (leftRes (sumAndCnt (-> node left)))
        (rightRes (sumAndCnt (-> node right)));

    ret (tup (+ (. leftRes 0) (. rightRes 0) (-> node val))
        (+ (. leftRes 1) (. rightRes 1) 1));
};

fnc main () () {
    sym (ll (makeNode 1 null null));
    sym (l (makeNode 3 (& ll) null));
    sym (rl (makeNode 0 null null)) (rr (makeNode 5 null null));
    sym (r (makeNode -1 (& rl) (& rr)));
    sym (root (makeNode 2 (& l) (& r)));

    sym (result (sumAndCnt (& root)));

    printf "%d %d\n" (. result 0) (. result 1);
};
```