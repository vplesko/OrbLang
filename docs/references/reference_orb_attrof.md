---
layout: default
title: AttrOf
---
# {{ page.title }}

Used to fetch the value of an attribute with the given name of a node.

## `attrOf val name<id>`

Returns the attribute named `name` from `val`. If `val` is a `type`, may perform a lookup on its type-specific attributes.

If name is `type`, the lookup refers to the type attribute of the node. Otherwise, the lookup is performed on the node's non-type attributes.

If an attribute is not found through the procedure above and `val` is a `type`, a lookup is performed on its type-specific attributes.

```
import "std/io.orb";

data Foo {
    x:i32
}::((xTy i32));

fnc main () () {
    eval (sym (x 0::mark));

    attrOf x mark;  # true
    attrOf Foo xTy; # i32
};
```

> There are two types of attributes: the type attribute (defined using `:`) and non-type attributes (defined using `::`).

> Each non-type attribute has a name and a value. The name must not be the identifier `type`. Node attributes may not share names. If the value is not specified, it is implicitly `true`.

> Attribute values must be evaluated, and of a non-owning type.

> Node attributes are preserved in invocation arguments, `raw` elements, and evaluated ref values.

> After a node is processed, if the original node had a type attribute, it overrides the type attribute of the resulting node. The same applies to non-type attributes (as a whole, **not** per individual attribute).