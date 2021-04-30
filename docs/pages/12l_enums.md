---
layout: default
title: Enums
---
# {{ page.title }}

Enums are types which have a set of named possible values. They are defined using the `enum` macro defined in **base.orb**.

```
enum CardSuite {
    Spades
    Hearts
    Clubs
    Diamonds
};
```

Values of this enum can then be referred to as, eg. `CardSuite.Spades`.

By default, the underlying type of an enum is `i32`, the first value is `0`, and each value other than first is one greater than the previous one.

You may override these defaults.

```
enum HttpStatus u32 {
    (Ok 200)
    Created
    Accepted
    (BadRequest 400)
    Unauthorized
    PaymentRequired
    Forbidden
    NotFound
    (InternalError 500)
};
```

You can get the number of different values in your enum using `base.enumSize`.

```
    std.println (base.enumSize CardSuite); # prints 4
```

> There is a convention to name your functions (and macros) as `MODULE_NAME.FUNCTION_NAME`. If the function (or macro) is not intended to be called directly by your users, its name should be prepended with a minus: `MODULE_NAME.-FUNCTION_NAME`. A lot of functions and macros from **base.orb** don't follow this convention for the sake of convenience.