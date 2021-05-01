# BinTreeMacro

In the previous example, we saw how we can define and work with data types. You may have gotten the impression that Orb's syntax gets unwieldy at times. Fortunately, there is a tool to aid you - macros.

Macros are procedures, in some ways similar to functions, which return Orb code which will get processed in place of their invocations.

## Idea

While there are a few places in the previous example that could be simplified, here we will focus on a single problem - providing convenient syntax for constructing binary trees.

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

We will write a macro `symBinTree`, which will allow us to build a binary tree with the syntax you see above. This is the same tree from the previous example. Later, we will simplify the syntax further.

When writing a macro, it is a good idea to first write out what it should expand into.

```
sym (G0 (makeNode 1 null null))
    (G1 (makeNode 3 (& G0) null))
    (G2 (makeNode 0 null null))
    (G3 (makeNode 5 null null))
    (G4 (makeNode -1 (& G2) (& G3)))
    (tree (makeNode 2 (& G1) (& G4)));
```

G0-4 represent some internally generated identifiers. Take the time to figure out how this code maps to the invocation above.

We will generate this instruction recursively. First, we will collect all `sym` entries needed to define the left subtree, then the right subtree. We will concatenate those, and append the entry for the root. Finally, we wrap everything in a `sym` form.

Recursion callers will provide the names for variables of left/right subtree's roots. If a subtree is empty (denoted by `()`), we will not make a recursive call.

## Implementation

First, let's open up the macro.

```
mac symBinTree (name tree) {
};
```

It takes two arguments. Macro arguments and return values are untyped, as they can be of any type (or some special untyped values). They always do return some value.

For recursion, we will define a function `symBinTreeEntries`. In it, we generate a list of `sym` entries for defining a subtree.

It will take two arguments. `name` will be the name under which we are to define the root of the subtree. `tree` will contain 3 values - `i32` value of the node, and descriptions of the left and right subtrees written by the caller of our macro.

```
eval (fnc symBinTreeEntries (name:id tree:raw) raw {
});
```

There are several things to note here. The first argument is of type `id`. This type represents an identifier.

The second argument is of type `raw`. Technically, a `raw` is a collection of any number of values of any type (or untyped). Practically, you can think of it as a chunk of Orb syntax that can be operated on.

(It's called `raw` because usually it represents unprocessed Orb code. Evaluations and compilations are both referred to as processing.)

The function returns a `raw` containing entries to be appended to the final `sym` statement.

Functions which work with `id`, `type`, or `raw` cannot be compiled. That is ok, we only need this function while user code is compiling. For this, we need to wrap it in `eval`.

`eval` tells the compiler that code within is meant for compile-time evaluations, and should not be compiled. Here, it means that this function is only available at compile-time.

Compare this to `::evaluable` which tells the compiler that a function can both be evaluated at compile-time, and executed at runtime.

We will declare a variable `childEntries` in which we will accumulate entries received from recursive calls.

```
    sym (childEntries ());
```

`()` is a `raw` with no elements - an empty `raw`.

We also declare `makeNodeCall` as a raw containing code for calling `makeNode` with all necessary arguments. For now, we only know one argument and that is the `i32` value of the tree node. We will append the other 2 arguments later on.

```
    sym (childEntries ())
        (makeNodeCall \(makeNode ,([] tree 0)));
```

`\` is used for escaping. When applied to a node in parenthesis, it states that the elements within (which also get escaped) are to be used in constructing a `raw` object.

If you escape an identifier, it will denote a simple `id` value. Otherwise, the compiler would perform a lookup and fetch the value it points to.

`,` is used for unescaping. This counteracts `\` and tells the compiler that this expression is actually supposed to be evaluated right now. We use it to fetch element 0 from `tree`.

Take note that while `makeNodeCall` contains actual code, `childEntries` is used as an extensible list of values. Both are valid uses of `raw` variables.

There are 2 subtrees, accessible with `[] tree 1` and `[] tree 2`. Both are handled the same way, so `range` with two bounds can be used.

```
    range i 1 2 {
    };
```

We need to know whether the subtree is described with `()`. First, let's ask if it is even a `raw` value.

```
        sym (isRaw (|| (== (typeOf ([] tree i)) raw) (== (typeOf ([] tree i)) (raw cn))));
```

`||` is a macro for logical OR. `typeOf` returns the type of a value.

Now, let's check if this raw is empty.

```
        if (&& isRaw (== (lenOf ([] tree i)) 0)) {
```

`&&` is a macro for logical AND. `lenOf` returns the number of elements in a `raw`. It can also return the number of elements in a tuple, or number of elements in an array.

In the case of `()`, don't make a recursive call, and use `null` as argument to `makeNode`.

```
        if (&& isRaw (== (lenOf ([] tree i)) 0)) {
            = makeNodeCall (+ makeNodeCall \( null ));
```

Using `+`, we can concatenate the children of multiple `raw`s, thus generating a new `raw` value.

In the case when the subtree does exist, recursively generate its entries and append them to `childEntries`. We append `(& G0)` as argument to `makeNodeCall`.

`if` can take an additional argument - the else clause. This is where we will insert the following code.

```
        } {
            sym (childName (genId));
            = childEntries (+ childEntries (symBinTreeEntries childName ([] tree i)));
            = makeNodeCall (+ makeNodeCall \( (& ,childName) ));
        };
```

Note the call to `genId` to generate the name of the subtree's root.

Using macros can be dangerous (look up hygienic macros to understand why). If the code you are returning defines new variables, make sure to use `genId` to generate their names.

The last thing left to do in `symBinTreeEntries` is return the concatenation of `childEntries` and the entry for the current node.

```
    ret (+ childEntries
        \( (,name ,makeNodeCall) ));
```

Finally, in `symBinTree` return a `sym` form which contains all the generated entries.

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

This is the same tree from above, but cluttered with fewer `()`s.

To accomplish this, we need to somehow handle the case when a subtree is described with a single integer, rather than a `raw`.

The solution turns out to be simple.

```
eval (fnc symBinTreeEntries (name:id val:i32) raw {
    ret \( (,name (makeNode ,val null null)) );
});
```

Adding this overloads `symBinTreeEntries` with another function. This one takes an `i32` for its second argument, instead of `raw`.

When calling an overloaded function, which one gets called is decided based on provided arguments and signatures of candidate functions.

You can inspect the final code to see how `symBinTree` is called. This can also be simplified. For example, the macro can be changed to allow defining multiple trees in one invocation, similar to how `sym` can declare multiple values.

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
    = ([] node val) val;
    = ([] node left) left;
    = ([] node right) right;
    ret node;
};

eval (fnc symBinTreeEntries (name:id val:i32) raw {
    ret \( (,name (makeNode ,val null null)) );
});

eval (fnc symBinTreeEntries (name:id tree:raw) raw {
    sym (childEntries ())
        (makeNodeCall \(makeNode ,([] tree 0)));

    range i 1 2 {
        sym (isRaw (|| (== (typeOf ([] tree i)) raw) (== (typeOf ([] tree i)) (raw cn))));

        if (&& isRaw (== (lenOf ([] tree i)) 0)) {
            = makeNodeCall (+ makeNodeCall \( null ));
        } {
            sym (childName (genId));
            = childEntries (+ childEntries (symBinTreeEntries childName ([] tree i)));
            = makeNodeCall (+ makeNodeCall \( (& ,childName) ));
        };
    };

    ret (+ childEntries
        \( (,name ,makeNodeCall) ));
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

    ret (tup (+ ([] leftRes 0) ([] rightRes 0) (-> node val))
        (+ ([] leftRes 1) ([] rightRes 1) 1));
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
    printf "%d %d\n" ([] result 0) ([] result 1);

    symBinTree tree1
        (2
            (3
                1
                ())
            (-1
                0
                5));

    = result (sumAndCnt (& tree1));
    printf "%d %d\n" ([] result 0) ([] result 1);

    symBinTree tree2 (3 () ());

    = result (sumAndCnt (& tree2));
    printf "%d %d\n" ([] result 0) ([] result 1);

    symBinTree tree3 3;

    = result (sumAndCnt (& tree3));
    printf "%d %d\n" ([] result 0) ([] result 1);
};
```