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

Let's say `module` is an owning non-ref value. When processing the returned code snippet, this same value will appear three different times. That means it will be dropped three times, resulting in undefined behaviour.

**If a macro argument may be an owning non-ref value, it should be returned exactly once.**

Following this rule will protect you from writing drop unsafe macros.

There is a circumstance that works in our favour.

```
mac printDetails (res::preprocess) {
    ret \(block {
        std.println ([] ,res resourceName);
        std.println ([] ,res countOfUses);
    });
};
```

`[]` cannot be used on owning non-ref values. Because of this, `res` must be ref, or the compiler will raise an error. Ref values are dropped only when their corresponding symbol goes out of scope. We may rest assured that this macro is drop safe.