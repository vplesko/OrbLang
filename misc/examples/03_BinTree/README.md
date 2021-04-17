# BinTree

In this example we will define `Node` - a node in a binary tree, and a function `sumAndCnt` which will calculate the sum and count of values in all nodes in the tree.

We can define `Node` as a data type. Data types are collections of named values of given types.

```
data Node {
    val:i32
    left:(Node *)
    right:(Node *)
};
```

`data` is used to define data types. `Node *` is a pointer to `Node`.

While we're at it, let's define a utility function for constructing a `Node`.

```
fnc makeNode (val:i32 left:(Node *) right:(Node *)) Node {
    sym node:Node;
    = ([] node val) val;
    = ([] node left) left;
    = ([] node right) right;
    ret node;
};
```

Elements of data types are accessed with `[]` operator.

Start writing `sumAndCnt`.

```
fnc sumAndCnt (node:(Node cn *)) (i32 i32) {
};
```

`Node cn *` is a pointer to a constant `Node`. The value it points to cannot be modified. Note that the pointer itself is not constant, though we could have declared it as such with `Node cn * cn`.

`(i32 i32)` is a tuple. In this case, it is a collection of two `i32` values. Tuple elements are also accessed using `[]`, but using numerical indexes instead of identifiers.

The first element of this tuple will be the sum, and second the count of all node values of the tree.

`sumAndCnt` will be a recursive function. Its base case is when `node` is `null`.

```
    if (== node null) {
        ret (tup 0 0);
    };
```

`tup` constructs a tuple out of given values.

Make recursive calls on left and right subtrees.

```
    sym (leftRes (sumAndCnt (-> node left)))
        (rightRes (sumAndCnt (-> node right)));
```

`->` is a macro. It is similar to `[]`, but used for accessing elements behind a pointer.

Finally, return the final value of the function.

```
    ret (tup (+ (. leftRes 0) (. rightRes 0) (-> node val))
        (+ (. leftRes 1) (. rightRes 1) 1));
```

`+`, like many other things in Orb, can handle many arguments. In this case, both `+`s perform addition on 3, instead of just 2 values.

To call `sumAndCnt` from `main`, we need to construct a binary tree. We will construct this one:

```
            2
    3               -1
1       X       0       5
```

, by using `makeNode`.

```
    sym (ll (makeNode 1 null null))
        (l (makeNode 3 (& ll) null))
        (rl (makeNode 0 null null))
        (rr (makeNode 5 null null))
        (r (makeNode -1 (& rl) (& rr)))
        (root (makeNode 2 (& l) (& r)));
```

`&` is used to get the pointer to a value. Alternatively, we could have used `malloc` to store these nodes on heap, but we would have to `free` them later.

Now, we can call `sumAndCnt` and print the result.

```
    sym (result (sumAndCnt (& root)));

    printf "%d %d\n" ([] result 0) ([] result 1);
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
    = ([] node val) val;
    = ([] node left) left;
    = ([] node right) right;
    ret node;
};

fnc sumAndCnt (node:(Node cn *)) (i32 i32) {
    if (== node null) {
        ret (tup 0 0);
    };

    sym (leftRes (sumAndCnt (-> node left)))
        (rightRes (sumAndCnt (-> node right)));

    ret (tup (+ ([] leftRes 0) ([] rightRes 0) (-> node val))
        (+ ([] leftRes 1) ([] rightRes 1) 1));
};

fnc main () () {
    sym (ll (makeNode 1 null null))
        (l (makeNode 3 (& ll) null))
        (rl (makeNode 0 null null))
        (rr (makeNode 5 null null))
        (r (makeNode -1 (& rl) (& rr)))
        (root (makeNode 2 (& l) (& r)));

    sym (result (sumAndCnt (& root)));

    printf "%d %d\n" ([] result 0) ([] result 1);
};
```