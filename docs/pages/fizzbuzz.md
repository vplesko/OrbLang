---
layout: default
title: FizzBuzz
---
# {{ page.title }}

We have learned enough to write a FizzBuzz program. This is a somewhat (in)famous programming exercise that is fairly simple to code. The problem statement is this (quoted from source):

*Write a program that prints the numbers from 1 to 100. But for multiples of three print "Fizz" instead of the number and for the multiples of five print "Buzz". For numbers which are multiples of both three and five print "FizzBuzz".*

We can use `range` to iterate from 1 to 100. To decide what to print we will use `if`, one body of instructions for each outcome.

```
import "base.orb";
import "std/io.orb";

fnc main () () {
    range i 1 100 {
        if (== (% i 15) 0) {
            std.println "FizzBuzz";
        } (== (% i 3) 0) {
            std.println "Fizz";
        } (== (% i 5) 0) {
            std.println "Buzz";
        } {
            std.println i;
        };
    };
};
```

Just like with *HelloWorld*, if you have **orbc** installed and the code above in a file called **main.orb**, you can compile and run this program with:

```
orbc main.orb
./main
```

This may be a simple program, but it nicely showcases usages of some important Orb concepts.