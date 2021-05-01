---
layout: default
title: Move semantics
---
# {{ page.title }}

Any value that has a drop function, or any of its descendant elements has a drop function, is considered to be an owning value. There are special restrictions regarding them.

The intent is to not allow multuple values to own the same resource. Because of this, owning values may not be copied.

```
fnc main () () {
    sym ctrl0:Control ctrl1:Control;

    # assign to ctrl0

    = ctrl1 ctrl0; # error!
};
```
{: .code-error}

This does not apply to non-ref values. They are about to be dropped anyway, instead of which their values may be copied off elsewhere.

```
fnc makeCtrl (str:String) Control {
    # ...
};

fnc main () () {
    sym ctrl:Control;

    # the call returns a non-ref owning value
    # this value can be taken over by ctrl
    = ctrl (makeCtrl "some resource");
};
```

To explicitly release ownership of a ref owning value, use the unary `>>` operator. This is known as the move operator.

Moving will reset the value to its zero state, while returning the owning value as a non-ref value.

```
fnc main () () {
    sym (ctrl0:Control (makeCtrl "some resource"));

    sym (ctrl1 (>> ctrl0));
    # ctrl1 now owns the resource, ctrl0 is in its zero state
};
```

Function arguments may be marked `::noDrop`. This tells the compiler that, even though the type is owning, the argument itself is not.

```
import "std/io.orb";

fnc printCtrl (ctrl:Control::noDrop) () {
    std.println "Controlling: " ([] ctrl str);

    # ctrl is not dropped
};

fnc main () () {
    sym (ctrl (makeCtrl "some resource"));

    # no need to move
    printCtrl ctrl;
};
```

For convenience, `pass` and `ret` will implicitly move their argument if it is about to go out of scope.

```
fnc makeCtrl (str:String) Control {
    sym ctrl:Control;
    = ([] ctrl str) str;

    # no need to move
    ret ctrl;
};
```

Sometimes, you may want to drop an owning value before it goes out of scope. You can accomplish this by using `>>` on that value, but not capturing the result.