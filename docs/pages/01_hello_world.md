---
layout: default
title: HelloWorld
---
# {{ page.title }}

As tradition dictates, we will start by writing a simple HelloWorld program. Here is the **main.orb** file containing the code:

```
import "std/io.orb";

fnc main () () {
    std.println "Hello, world!";
};
```

Let's dissect all that's happening here. First, we import **std/io.orb** from the standard library. This file contains various definitions for reading from the standard input and writing to the standard output.

Then we define a main function, using the `fnc` special form. This function is the entry point to our program. There are a few different signatures it may have, but here it simply takes no arguments and returns no values.

Inside its body, we use `std.println` to output a string, followed by a newline character.

To compile and execute this program, run these commands from the same directory that contains **main.orb** (it is assumed you have **orbc** installed on your system):

```
orbc main.orb
./main
```

When the program executes, you should see *Hello, world!* printed to the console.

Congratulations, you wrote your first Orb program! Click next to proceed to the next lesson.