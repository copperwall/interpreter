# clox example programs

Larger Lox programs that exercise classes, inheritance, dynamic dispatch,
closures and recursion. Each one prints deterministic output, checked against
`expected/`.

| Program | Shows off |
| --- | --- |
| `01_shapes.lox` | Class hierarchy three levels deep, overriding, `super`, template methods, comparator closures, bound methods |
| `02_symbolic.lox` | Expression tree: eval, pretty-print, symbolic differentiation and simplification via polymorphic nodes |
| `03_visitor.lox` | Visitor pattern (double dispatch) over a file-system tree; fluent builders |
| `04_nqueens.lox` | Recursive backtracking over immutable cons lists; closures collecting results |
| `05_bst.lox` | Binary search tree: recursive insert/delete/traversals, bound methods as callbacks |
| `06_closures.lox` | Counters, generators and generator combinators, message-passing objects, shared upvalues, loop-capture gotcha |
| `07_functional.lox` | map/filter/reduce, compose, curry, flip, partial, memoize, once |
| `08_church.lox` | Church booleans, numerals, pairs, predecessor, and the Z combinator |
| `09_sorting.lox` | Merge sort, quicksort, insertion sort on linked lists; seeded PRNG; stability |
| `10_life.lox` | Conway's Game of Life as an object graph of cells on a torus |
| `11_events.lox` | Event emitter / observer pattern with closures, bound methods, once-handlers, unsubscribe |
| `12_stack_vm.lox` | A Forth-like stack machine in Lox: polymorphic instructions, fluent assembler, call/return |
| `13_mandelbrot.lox` | ASCII Mandelbrot and Julia sets (floating point, nested loops) |

## Running

```sh
make                                    # from the clox directory
examples/run.sh                         # run all, diff against expected/
CLOX=build/release/clox examples/run.sh # with another build
examples/run.sh 05_bst.lox              # just one
examples/run.sh --update                # regenerate expected output
```

The suite also passes under `-DDEBUG_STRESS_GC` with ASan/UBSan, so it doubles
as a GC regression test.

## Working within clox's limits

Lox has no number-to-string conversion, so most examples embed
`prelude.lox`, which has `floor`, `mod`, `str`, `fixed` and `fmt`. There are no
imports, so each copy is pasted in. After editing `prelude.lox`, run
`./sync_prelude.py` to update the copies.

Other constraints that shaped the code are listed, with suggested
fixes, in [`../NEW_FEATURES.md`](../NEW_FEATURES.md):

- There are no arrays or maps, so everything is built from cons lists.
- The VM has only 64 call frames, so list functions loop instead of recursing.
- Each chunk holds at most 256 constants, so driver code goes in `main()`.
