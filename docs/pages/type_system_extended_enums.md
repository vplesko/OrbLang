---
layout: default
title: Enums
---
# {{ page.title }}

Enums are types with a set of possible values. They are defined using the `enum` macro from **base.orb**.

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

You can get the number of different values in your enum using `base.getEnumSize`.

```
    std.println (base.getEnumSize CardSuite); # prints 4
```