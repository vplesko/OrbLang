---
layout: default
title: Escaping
---
# {{ page.title }}

In this section, we will only explain how escaping and unescaping are done. What it is actually used for will be shown in the next section.

Each node (whether leaf or non-leaf) has an escape score. By default, the escape score is zero and the node is not escaped.

To increase the node's escape score, prepend it with `\`. If it's a non-leaf node, this escaping is propagated to all nodes nested within it.

To decrease a node's escape score, prepend `,` to it. If it's a non-leaf node, unescaping is also propagated to its nested nodes.

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