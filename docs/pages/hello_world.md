---
layout: default
title: HelloWorld
---
# HelloWorld

As tradition dictates, we will start by writing a simple hello-world program. Here is a **main.orb** file containing the code:

```
import "std/io.orb";

fnc main () () {
    std.println "Hello, world!";
};
```

Let's dissect all that's happening here. First, we import **std/io.orb** from the standard library. This file contains various definitions for reading from the standard input and writing to the standard output.

Then we define a main function, using `fnc` special form. This function is the entry point to Orb programs. There are a few signatures it may have, but here it simply takes no arguments and returns no values.

In its body, main uses `std.println` to output a string, followed by a newline character.

To compile and execute this program, run these commands from the same directory that contains **main.orb** (it is assumed you have **orbc** installed on your system):

```
orbc main.orb
./main
```

When the program executes, you should see *Hello, world!* printed to the console.

Congratulations, you wrote your first Orb program! Click next to proceed to the next lesson.