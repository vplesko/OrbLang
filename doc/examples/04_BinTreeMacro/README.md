# BinTreeMacro

In the previous example, we saw how to define and work with data types. You may have gotten the impression that Orb's syntax gets unwieldy at times. Fortunately, there is a tool to aid you - macros.

Macros are procedures, in some ways similar to functions, which return Orb code which will get processed in place of their invocations.

## Idea

While there are a few places in the previous example that could be simplified, here we will focus on a single problem - providing a better syntax for constructing binary trees.

```
symBinTree tree
    (2
        (3
            (1
                ()
                ())
            ())
        (-1
            (0
                ()
                ())
            (5
                ()
                ())));
```

We will write a macro `symBinTree`, which will allow us to build a binary tree with the syntax you see above. This is the same tree from the previous example. Later, we will improve the syntax further.

When writing a macro, it is a good idea to first write out what it should expand into.

```
sym (G0 (makeNode 1 null null))
    (G1 (makeNode 3 (& G0) null))
    (G2 (makeNode 0 null null))
    (G3 (makeNode 5 null null))
    (G4 (makeNode -1 (& G2) (& G3)))
    (tree (makeNode 2 (& G1) (& G4)));
```

G0-4 represent some internally generated identifiers.

We will generate this recursively. First, we will collect all `sym` entries needed to define the left subtree, then the right subtree. We will concatenate those entries, and append the entry for the root. Finally, we wrap everything in a `sym` form.

Recursive calls will provide the names for variables of left/right subtree's roots. If a subtree is empty (denoted by `()`), we will not generate any entries, and just use `null` in our call to `makeNode` instead.

## Implementation

First, let's open up the macro.

```
mac symBinTree (name tree) {
};
```

It takes two arguments. Macro arguments are untyped, as they can accept values of any type (and some special untyped values). They return elements of Orb syntax, which can also vary in their type, so are not specified either.

For recursion, we will define a function `symBinTreeEntries`.

```
eval (fnc symBinTreeEntries (name:id tree:raw) raw {
```

There are several things going on here. This function takes an argument of type `id`. This type represents an identifier, its value will be the name under which we want the root of the subtree to be declared.

The second argument is of type `raw`. Technically, a `raw` is a collection of any number of values of any type (or untyped). Practically, you can think of them as chunks of Orb syntax that can be operated on. This argument will be a piece of code provided by the user of our macro.

The function returns a `raw` containing as elements entries to be appended to the final `sym` statement.

Functions which work with `id`, `type`, or `raw` cannot be compiled. That is ok, we only need this function while user code is compiling.

We surround it in `eval`. `eval` tells the compiler that a code needs to be evaluated, rather then compiled. Here, it means that this function is only usable at compile-time, and will not be compiled. Compare this to `::evaluable` which tells the compiler that a function can both be evaluated at compile-time, and executed at runtime.

```
    sym (childEntries ()) (makeNodeCall \(makeNode ,(. tree 0)));
```

`()` is a `raw` with no elements - an empty `raw`. We will be appending elements to `childEntries` soon. It represents entries in `sym` needed to construct our node's left and right subtrees.

`makeNodeCall` is a `raw`, but not an empty one. It will contain the code for making a call to `makeNode` with all needed arguments.

`\` is used for escaping - it means that the elements within (which also get escaped) are to be used in constructing a `raw` object. If you escape an identifier, it will denote a simple `id` value. Otherwise, the compiler would perform a lookup and fetch the value it points to.

`,` is used for unescaping. This counteracts `\` and tells the compiler that this expression is actually supposed to be evaluated right now.

This all sums up into `\(makeNode ,(. tree 0))`. This is a piece of code that would make a call to `makeNode` with the first value from `tree` - the value of the current node. Remember, we are giving 3 elements in each node - node's `i32` value, description of the left subtree, and description of the right subtree. The other 2 arguments will be appended soon.

If the subtree is described with `()` it is nonexistent. We denote this with `null` in our `Node`.

```
    range i 1 2 {
        sym (isRaw (|| (== (typeOf (. tree i)) raw) (== (typeOf (. tree i)) (raw cn))));

        if (&& isRaw (== (lenOf (. tree i)) 0)) {
```

There are 2 subtrees, accessible with `. tree 1` and `. tree 2`. Both are handled the same way, so `range` is used.

If `lenOf` of a `raw` is 0, then its value is `()`. In this case we append `null` as argument to `makeNodeCall`.

Using `+`, we can concatenate the children of multiple `raw`s, thus generating a new `raw` value.

```
            = makeNodeCall (+ makeNodeCall \( null ));
```

In the case when the subtree does exist, we recursively generate its entries and append them to `childEntries`. We append `(& G0)` as argument to `makeNodeCall`.

`if` can take an additional argument - the else clause. This is where we will insert the following code.

```
        } {
            sym (childName (genSym));
            = childEntries (+ childEntries (symBinTreeEntries childName (. tree i)));
            = makeNodeCall (+ makeNodeCall \( (& ,childName) ));
        };
```

Using macros can be dangerous (look up hygienic macros to understand why). If the code you are returning defines new variables, make sure to use `genSym` to generate their names. We do this for our node's children.

The last thing we do in `symBinTreeEntries` is return the concatenation of `childEntries` and the entry for the current node (the root of the recursive subtree).

```
    ret (+ childEntries \( (,name ,makeNodeCall) ));
```

Finally, in `symBinTree` we return a `sym` form which contains all the generated entries.

```
mac symBinTree (name tree) {
    ret (+ \( sym ) (symBinTreeEntries name tree));
};
```

## Improvement

We want to further simplify our syntax for generating binary trees.

```
symBinTree tree0
    (2
        (3
            1
            ())
        (-1
            0
            5));
```

This is the same tree from above, but written with fewer `()`s.

To accomplish this, we need to somehow handle the case when a subtree is a single integer, rather than a `raw`.

The solution turns out to be simple.

```
eval (fnc symBinTreeEntries (name:id val:i32) raw {
    ret \( (,name (makeNode ,val null null)) );
});
```

This overloads `symBinTreeEntries` with a new function. This one takes an `i32` for its second argument, instead of `raw`.

When calling an overloaded function, which one gets called depends on provided arguments and signatures of the candidate functions.

You can inspect the final code for how `symBinTree` is tested. It can also be simplified. For example, the macro can be changed to be able to define multiple trees in one invocation, similar to how `sym` can declare multiple values.

## Final code

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

eval (fnc symBinTreeEntries (name:id val:i32) raw {
    ret \( (,name (makeNode ,val null null)) );
});

eval (fnc symBinTreeEntries (name:id tree:raw) raw {
    sym (childEntries ()) (makeNodeCall \(makeNode ,(. tree 0)));

    range i 1 2 {
        sym (isRaw (|| (== (typeOf (. tree i)) raw) (== (typeOf (. tree i)) (raw cn))));

        if (&& isRaw (== (lenOf (. tree i)) 0)) {
            = makeNodeCall (+ makeNodeCall \( null ));
        } {
            sym (childName (genSym));
            = childEntries (+ childEntries (symBinTreeEntries childName (. tree i)));
            = makeNodeCall (+ makeNodeCall \( (& ,childName) ));
        };
    };

    ret (+ childEntries \( (,name ,makeNodeCall) ));
});

mac symBinTree (name tree) {
    ret (+ \( sym ) (symBinTreeEntries name tree));
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
    symBinTree tree0
        (2
            (3
                (1
                    ()
                    ())
                ())
            (-1
                (0
                    ()
                    ())
                (5
                    ()
                    ())));

    sym (result (sumAndCnt (& tree0)));
    printf "%d %d\n" (. result 0) (. result 1);

    symBinTree tree1
        (2
            (3
                1
                ())
            (-1
                0
                5));

    = result (sumAndCnt (& tree1));
    printf "%d %d\n" (. result 0) (. result 1);

    symBinTree tree2
        (3 () ());

    = result (sumAndCnt (& tree2));
    printf "%d %d\n" (. result 0) (. result 1);

    symBinTree tree3 3;

    = result (sumAndCnt (& tree3));
    printf "%d %d\n" (. result 0) (. result 1);
};
```