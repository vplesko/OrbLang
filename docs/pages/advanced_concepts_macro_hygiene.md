---
layout: default
title: Macro hygiene
---
# {{ page.title }}

We mentioned that one of the potential dangers when writing macros is that they are unhygienic. This is not specific to Orb - looking up macro hygiene online would give you plenty of reading material on the topic.

So, what is the danger exactly? Let's look at the following naive implementation of the `repeat` macro:

```
mac myRepeat (n body) {
    ret \(range i ,n ,body);
};
```
{: .code-error}

Seems good? Now let's look at this code:

```
import "std/io.orb";

fnc main () () {
    range i 1 5 {
        myRepeat 3 {
            std.print i;
        };
    };
};
```
{: .code-error}

The user is expecting this code to print `111222333444555`. What it actually prints is `012012012012012`.

If you look at our definition of `myRepeat`, you will see we used `i` to name the iterator variable. The user of our macro also used `i` to name their own variable. Due to how shadowing works, our `i` is the one being caught in the nested block.

Orb's macros are unhygienic because they can interfere with the user's naming scheme. There are multiple types of scenarios where issues like these can show up, and some of them can be surprising. It is advised to read more about macro hygiene to really understand the issue.

One way we can fix `myRepeat` is to use a unique identifier for the iterator variable and use a different identifier on every invocation.

There is a function in **base.orb** which does exactly what we need. `genId` returns a different identifier on every call. So, we should rewrite `myRepeat` as:

```
mac myRepeat (n body) {
    sym (myId (genId));
    ret \(range ,myId ,n ,body);
};
```