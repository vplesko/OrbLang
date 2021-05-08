---
layout: default
title: IsEval
---
# {{ page.title }}

Used to check whether a value is evaluated or if current processing is done through evaluation.

## `isEval val -> bool`

Returns a `bool` of whether `val` is an evaluated value.

`val` must be a typed value.

The result is an evaluated value.

```
fnc main () () {
    sym x:i32;
    eval (sym y:i32);

    isEval x; # false
    isEval y; # true
};
```

## `isEval -> bool`

Returns whether this instruction was processed through evaluation.

The result is an evaluated value.

```
fnc main () () {
    isEval;        # false
    eval (isEval); # true
};
```