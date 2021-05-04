---
layout: default
title: Automatic cleanup
---
# {{ page.title }}

Managing resources, most notably memory, in code can be toilsome and error-prone. Fortunately, Orb comes with scope-based memory management. This means that types that manage memory can be set up to "clean up after themselves" by defining special functions. These are called drop functions, and can only be defined for data types.

To define a drop function, pass it as an extra argument when defining a data type. This argument must be a function that takes a single argument of that data type, marked with `::noDrop`.

This is best illustrated by an example.

```
data Control {
    str:String
} (lam (this:Control::noDrop) () {
    std.println "Releasing: " ([] this str);
});
```

Drop functions are called as a symbol goes out of scope. Additionally, each of their elements are also dropped. As a rule, the order of dropping is inverse to the order in which the values were defined.

This may sound a bit complicated, but it simply means that the compiler will take care of automatically cleaning up any resources you are using, and you don't need to worry about them. Your responsibility is to define the drop function.

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

There is another rule that may be worth remembering - if a value is guaranteed to be in its zero state, the compiler *may* omit dropping it. Practically speaking, if a value is zero-initialized, it is considered to not own any resources.