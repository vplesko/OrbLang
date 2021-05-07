---
layout: default
title: Alias
---
# {{ page.title }}

The `alias` macro from **base.orb** can be used to refer to a type using a different name. This is useful for long type expressions, or types you anticipate wanting to refactor in the future.

```
alias Transformation (f32 4 4);
```

Aliases are defined only in the scope in which you invoke the `alias` macro. This is usually done from the global scope.

> There is a convention to start user-defined types and aliases with an uppercase letter.