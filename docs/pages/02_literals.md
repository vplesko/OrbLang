---
layout: default
title: Literals
---
# Literals

Literals are values of primitive types declared through Orb syntax. Here are some examples:

```
signed integers: 0 127 -9 +11

floating point values: 123.45 +0.01 1. -0.5

characters: 'A' '0'

booleans: true false

strings: "a string" ""

null pointer: null
```

Numeric literals (signed integers and floating point values) are written in decimal form by default, but can be written in other bases:

```
binary form prefixed with 0b: 0b0110 -0b10

octal form prefixed with 0: 0755 +0111

hexadecimal form prefixed with 0x or 0X: 0x7fff 0XBadF00d
```

You can use underscores in numeric literals and scientific notation in floating point literals:

```
underscores: 10_000 .0000_0001

decimal scientific notation: 1e2 1E-4

hexadecimal scientific notation (power to base 2): 0x.123p1 0x0.456P10
```

You can control the type of your literals by specifying a type attribute on them. What different types there are will be explained soon.

```
8-bit unsigned integer: 1:u8

64-bit signed integer: 100:i64

32-bit floating point value: 0.:f32
```

To use special characters in your character and string literals, you need to use character escaping. Same rules apply to both literal types.

```
single quote: '\''
double quote: '\"'
question mark: '\?'
backslash: '\\'
newline: '\n'
carriage return: '\r'
tab: '\t'
terminal: '\0'

by hex code: '\x41' '\x0A'

others: '\a' '\b' '\f' '\v'
```

There is one other type of literal - identifiers. These will make more sense later when we've explained escaping, but here some examples:

```
identifiers: x y val01 myId snake_case kebab-case

also valid identifiers: ?= <<< .. !!!

another valid identifier: a=+-*/%<>&|^!~[]._?0123456789
```