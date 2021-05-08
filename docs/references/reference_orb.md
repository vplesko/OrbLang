---
layout: default
title: 'Reference: Orb'
---
# {{ page.title }}

These sections document special forms of the Orb programming language. Each section documents a single special form, explaining how its used, what it does, and any additional options it may have. Possible usages are given through the names of arguments.

Argument details are sometimes placed inside angle braces `<>`. These may refer to the required type of the argument, or a loose description of what the argument must be. For example, `<id>` denotes an identifier, `<integer>` a signed or unsigned integer, `<block>` a `raw` with zero or more instructions, and `()` an empty `raw`.

Appending `...` to an argument name denotes that there may be one or more values given. Placing an argument in square braces `[]` denotes an optional argument.

If there is a guarantee on what is returned, it will be described at the end after `->`. If it is not specified, there may still be a return value explained in the text.

Code examples may be given for better illustration.