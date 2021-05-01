---
layout: default
title: Macro drop unsafety
---
# {{ page.title }}

The last danger to keep in mind when writing macros is drop unsafety. Let's take a look at this example:

```
mac pickOne (ind::preprocess args::(variadic preprocess)) {
    ret ([] args ind);
};
```
{: .code-error}

Now let's say we called this macro with an index and a list of owning non-ref values. One of them will be returned to our code, and probably dropped at some point. What will happen to the other ones? Well, nothing - they will not be dropped.

Just like `raw` values don't drop their elements, **macros do not own their arguments and will not drop them when they are done executing**!

Let's look at another example:

```
mac callPipeline (module::preprocess) {
    ret \(block {
        liquidize ,module;
        energize ,module;
        quantify ,module;
    });
};
```
{: .code-error}

Let's say `module` is an owning non-ref value. When processing the returned code snippet, this same value will appear three different times. That means it may end up being dropped three times, resulting in undefined behaviour.

**If a macro argument may be an owning non-ref value, it should be returned exactly once.**

Following this rule will protect you from writing drop unsafe macros.

There is another detail to know, which actually work in our favour.

```
mac printDetails (res::preprocess) {
    # verify res is of the expected type

    ret \(block {
        std.println ([] ,res resourceName);
        std.println ([] ,res countOfUses);
    });
};
```

`[]` cannot be used on owning non-ref values. Because of this, `res` would have to be ref, otherwise the compiler would raise an error. Because of that, it does not need to and will not be dropped in the returned code snippet. We may rest assured that this macro is drop safe.