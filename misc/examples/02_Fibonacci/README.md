# Fibonacci

We want to define a function `fibonacci`, which calculates the N-th number in the Fibonacci sequence.

```
fnc fibonacci (n:u32) u32 {
};
```

This function takes a single argument of type `u32` (32-bit unsigned integer) and returns a value of the same type.

If `n` is 0 or 1, immediately return the Fibonacci number.

```
    if (< n 2) {
        ret n;
    };
```

If not, calculate the number by starting with the 0th and 1st Fibonacci numbers, and iteratively moving through the sequence.

```
    sym (a (arr u32 0 1 1));
```

`sym` is used to declare variables. `arr` is a macro for constructing arrays. Here, `a` is an array of 3 `u32`s, whose starting values are 0, 1, and 1.

Element at index 2 of `a` will be the latest calculated number in the sequence. Let's repeatedly calculate the next Fibonacci number, then update the first two elements of `a` to prepare for the next iteration.

```
    repeat (- n 1) {
        = ([] a 2) (+ ([] a 0) ([] a 1));
        
        = ([] a 0) ([] a 1);
        = ([] a 1) ([] a 2);
    };
```

`repeat` is a macro which repeats a block of instructions a given number of times. `=` is the assignment operator. `[]` is the indexing operator.

Finally, return the N-th Fibonacci number.

```
    ret ([] a 2);
```

Now we can call this function from main.

```
fnc main () () {
    printf "%u\n" (fibonacci 20);
};
```

This works and should print 6765. However, we can ask the compiler to precalculate this number during compilation, rather than have the program waste time while running. To do this, we will use the `evaluable` function attribute.

```
fnc fibonacci::evaluable (n:u32) u32 {
```

`::` is used to apply attributes to syntax elements. `:` is also used for attributes - specifically for type attributes.

`evaluable` tells the compiler that this function can be evaluated at compile-time. Of course, this can only be done when the arguments are known at compile-time. If not, its execution will take place at runtime.

## Final code

```
import "base.orb";
import "clib.orb";

fnc fibonacci::evaluable (n:u32) u32 {
    if (< n 2) {
        ret n;
    };

    sym (a (arr u32 0 1 1));

    repeat (- n 1) {
        = ([] a 2) (+ ([] a 0) ([] a 1));
        
        = ([] a 0) ([] a 1);
        = ([] a 1) ([] a 2);
    };

    ret ([] a 2);
};

fnc main () () {
    printf "%u\n" (fibonacci 20);
};
```