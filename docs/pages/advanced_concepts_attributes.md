---
layout: default
title: Attributes
---
# {{ page.title }}

Attributes can be placed on a node to add extra information. There are two types of attributes.

`:` is used to attach a type attribute to a node. We've used these in `sym` instructions and function arguments.

```
    sym x:i32;
```

`::` is used to attach non-type attributes to a node. For example, we've seen `::noDrop` on function arguments and `::preprocess` on macro arguments.

There can be multiple non-type attributes attached to a single node. Each has a name and a value. If the value is not specified, it will implicitly be set to `true`.

```
# single attribute with no value provided
a::attr0

# single attribute with a value provided
a::((attr0 val0))

# multiple attributes with no values provided
a::(attr0 attr1)

# multiple attributes, some have values provided
a::(attr0 (attr1 val1) (attr2 val2))
```

The `attr??` and `attrOf` special forms are used for attribute inspection. `attr??` checks whether a node has an attribute with a given name. `attrOf` fetches the value of that attribute from the node.

The name of type attribute is `type`. Note that a node's type attribute is a different concept from the type of that node.

```
fnc main () () {
    eval (sym (x 0::((myAttr 10))));

    attr?? x myAttr; # true
    attrOf x myAttr; # 10

    attr?? x fakeAttr; # false

    attr?? 0:i32 type; # true
    attrOf 0:i32 type; # i32
};
```

Data types can have attributes attached to their type. This is useful for additional type inspection.

```
import "std/io.orb";

data Foo {
    x:i32
}::((xTy i32));

fnc main () () {
    sym foo:Foo;

    eval (if (attr?? (typeOf foo) xTy) {
        message (attrOf (typeOf foo) xTy); # prints i32
    });
};
```

Attributes are useful in conjunction with macro arguments, as they preserve their attributes during the invocation.

```
import "base.orb";

mac myFoo (a) {
    if (&& (attr?? a selection) (== (attrOf a selection) diffBehaviour)) {
        # ...
    };

    # ...
};
```