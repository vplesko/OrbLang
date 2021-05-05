---
layout: default
title: Explicit types
---
# {{ page.title }}

Explicit types refer to an existing type by a different name. They are similar to aliases, but with two key differences.

First, explicit types are brand new types and cannot be implicitly cast from or into other types, including the type they were built from.

Second, they can only be defined in the global scope.

They are defined using the `explicit` special form.

```
explicit TableId u64;
```

The reason to use them over aliases is that they provide extra type-safety for the newly introduced type. For example, if you have several different storage types, you may want to define different key types to index into them. Defining these as explicit types lets the compiler ensure you don't use a key of one storage to index another one.