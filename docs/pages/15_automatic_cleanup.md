---
layout: default
title: Automatic cleanup
---
# {{ page.title }}

When defining a data type, a drop function may be specified for it. This function must take a single argument of that data type, and it must be marked `::noDrop`.

The intent of this drop functions is to automatically release a resource for which the data type is responsible. This may be allocated memory, an open file, a mutex...

```
import "std/io.orb";

data Control {
    str:String
} (lam (this:Control::noDrop) () {
    std.println "Releasing: " ([] this str);
});
```

Drop functions are called as a value goes out of use (eg. when a symbol goes out of scope). As a rule, the order of dropping is inverse to the order in which the values were defined. Drop functions will also be called for elements descended from values no longer being used.

This may sound a bit complicated, but it simply means that the compiler will take care of automatically cleaning up any resources you are using, and you don't need to worry about it. Your responsibility is to define the drop function.

There is another rule that may be worth remembering - if a value is guaranteed to be in its zero state, the compiler *may* omit dropping it. So, if a value is zero initialized, it is considered to now own any resources.

```
fnc makeCtrl (str:String) Control {
    # ...
};

fnc main () () {
    sym ctrl:Control;
    # assign a value to ctrl

    sym ctrlArr:(Control 4);
    # assign values to elements of ctrlArr

    # this Control is not stored anywhere, so it is immediately dropped
    makeCtrl "some resource";

    # end of function scope:
    #   elements of ctrlArr are dropped in reverse order
    #   ctrl is dropped
};
```