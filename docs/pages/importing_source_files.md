---
layout: default
title: Importing source files
---
# {{ page.title }}

As your code grows larger, you may want to split it across multiple source files. To import the definitions from a file into your project, use the `import` special form, providing the file path as a string. We've already used it to import some Orb libraries, for example as `import "std/io.orb";`.

Importing a file will cause all nodes within it to be processed. The compiler remembers which files it already processed, so importing a file more than once has no additional effect.

This is true for the compilation as a whole, and it is not required to import a file if it was already imported from a different file. This means that, for example, you can import **base.orb** at the start of your **main.orb** file, and then never need to import it again in any of your other source files.

When calling **orbc** from the command line, you may specify multiple **.orb** files, and the compiler will process all of them. The compiler will always process any imported files, regardless of whether you listed them when calling it. So, you may simply execute `orbc main.orb` and have it pull in all of the files your project needs.

`import` looks for the source file path from the following locations (in order):
 - current directory where **orbc** was called
 - search paths given with the `-I` flag (in order)
 - directory where Orb libraries were installed