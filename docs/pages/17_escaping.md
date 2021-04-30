---
layout: default
title: Escaping
---
# {{ page.title }}

In this section, we will only explain how escaping and unescaping is done. What it is actually used for will be elaborated in the next section.

Each node (whether leaf or non-leaf) has an escape score. By default, the escape score is zero and the node is not escaped.

To increase the node's escape score, prepend `\` to it. If it's a non-leaf node, this escaping is propagated to all its descendants (its elements, elements of its elements...).

To decrease a node's escape score, prepend `,` to it. If it's a non-leaf node, unescaping is also propagated to its descendants.

If a node's escape score is greater than zero, it is considered to be escaped.

```
\( # escaped
    a # escaped
    ,b # not escaped
    \( # escaped
        c # escaped
        ,d # escaped
        ,,e # not escaped
    )
    ,( # not escaped
        a # not escaped
        \b # escaped
    )
)
```