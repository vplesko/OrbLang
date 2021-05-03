---
layout: default
title: Import
---
# {{ page.title }}

Used to import definitions from a source file into the current project.

## `import file<c8 cn []>`

Switches context to process nodes in the file given by path `file`. Once finished, returns to this file to process proceeding nodes.

`file` must be an evaluated value.

This instruction must not be nested within other instructions.

If the file was already processed, will not process it again. Importing must not cause a cyclical dependency between source files.