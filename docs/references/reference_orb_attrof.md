---
layout: default
title: AttrOf
---
# {{ page.title }}

Used to fetch the value of an attribute with the given name of a node.

## `attrOf val name<id>`

Returns the attribute named `name` from `val`. If `val` is a `type`, may perform lookup on its type specific attributes.

If name is equal to identifier `type`, the lookup refers to the type attribute of the node. Otherwise, the lookup is performed on the node's non-type attributes.

If an attribute is not found through the procedure above and `val` is a `type`, a lookup is performed on its type specific attributes.

> Attributes are either the type attribute or non-type attributes.

> Non-type attributes have a name and a corresponding value. The name must not be the identifier `type`. Multiple attributes of the same node may not have the same name. The value must be of a non-owning type. If the value is not specified, it is implicitly `true`.

> Attributes are preserved on invocation arguments and evaluated ref values. After a node is processed, if the original node had a type attribute, it overrides the type attribute of the resulting node. The same applies to non-type attributes (as a whole, **not** per individual attribute).