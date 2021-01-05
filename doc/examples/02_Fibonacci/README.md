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

If not, calculate the number by starting with the 0th and 1st Fibonacci numbers, and iteratively move through the sequence.

```
    sym (a:(u32 3));
    = ([] a 0) 0;
    = ([] a 1) 1;
```

`sym` is used to declare variables. Here, `a` is an array of 3 `u32`s. Its element at index 2 will be the latest calculated number in the sequence. `[]` is the indexing operator.

```
    times (- n 1) {
        = ([] a 2) (+ ([] a 0) ([] a 1));
        
        = ([] a 0) ([] a 1);
        = ([] a 1) ([] a 2);
    };
```

`times` is a macro which repeats a block of instructions a given number of times. In the body, we calculate the next Fibonacci number, and shift the elements in `a` using the assignment operator `=`.

Return the Fibonacci number.

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

`evaluable` tells the compiler that this function can be evaluated at compile-time. Of course, this can only be done when the arguments are known at compile-time.

## Final code

```
import "base.orb";
import "clib.orb";

fnc fibonacci::evaluable (n:u32) u32 {
    if (< n 2) {
        ret n;
    };

    sym (a:(u32 3));
    = ([] a 0) 0;
    = ([] a 1) 1;

    times (- n 1) {
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