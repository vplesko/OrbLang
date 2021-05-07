---
layout: default
title: Literals
---
# {{ page.title }}

Literals are values of primitive types declared through Orb syntax. One group of literals are signed integer literals.

```
import "std/io.orb";

fnc main () () {
    std.println 1;
    std.println +5;
    std.println -201;
    std.println 0;

    # binary form
    std.println 0b0110;

    # octal form
    std.println 0755;

    # hexadecimal form
    std.println 0x7fff;

    # underscores can be used in literals
    std.println 10_000_000;
};
```

Then, there are floating-point literals.

```
import "std/io.orb";

fnc main () () {
    std.println 123.45;
    std.println 1.;
    std.println -0.5;

    # scientific notation
    std.println 0.1e2;
    std.println 1.0E-4;
};
```

You can specify the type of your literals. (Orb's type system will be explained in more detail later.)

```
import "std/io.orb";

fnc main () () {
    # 8-bit unsigned integer
    std.println 1:u8;

    # 64-bit signed integer
    std.println -100:i64;

    # 64-bit floating-point value
    std.println 10.0:f64;
};
```

Character literals are placed between single quotes, for example: `'A'`, `'5'`, `'.'`, and `' '`. Some characters require special escape sequences: `'\n'` (newline), `'\t'` (tab), `'\\'` (backslash), `'\''` (single quote), `'\"'` (double quote), and `'\0'` (null character).

String literals are placed between double quotes, eg. `"Hello!"` and `""` (empty string). Strings use the same escape sequences as character literals.

String literals can spread multiple lines.

```
    sym (str "
            this is a
            multiline
            string
            literal");
```

`true` and `false` are boolean literals.

`null` is the null pointer literal.

There is one other type of literals - identifiers. These can contain alphanumerics and any of these characters: `=+-*/%<>&|^!~[]._?`. Therefore, all of the following are valid identifiers: `x`, `_foo`, `myId`, `val01`, `snake_case`, `kebab-case`, `*shiny*`, `?=`, `<<<`, `-||-`.