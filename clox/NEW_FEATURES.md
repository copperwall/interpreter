# New features wishlist

Features that would have made the programs in `examples/` simpler, found
while writing them. Each entry says where it hurt and what the workaround was.
Roughly ordered by how often it came up.

## 1. Number-to-string conversion

`"n = " + 3` is a runtime error ("Operands must be two numbers or two
strings"), so there is no way to put a number into a string.

- **Hit in:** every example except `13_mandelbrot`.
- **Workaround:** `prelude.lox` builds strings digit by digit with `str(n)`,
  `fixed(n, places)` and `fmt(n)`, using a hand-written `floor`.
- **Options:** a `str()` / `toString()` native; let `+` convert a number
  when the other operand is a string; or string interpolation
  (`"n = ${n}"`).

## 2. `print` loses precision on numbers

`printValue` uses `%g`, which keeps 6 significant digits:
`print 2540170;` prints `2.54017e+06`, and `print 3628800;` prints
`3.6288e+06`. Any integer over 999999 prints wrong.

- **Hit in:** factorial in `07_functional`, `08_church` and `12_stack_vm`; byte
  totals in `03_visitor`. The examples avoid it by going through `str()`.
- **Fix:** print integral values with `%.0f` (or as `int64_t`), and use
  `%.17g` (or the shortest round-tripping form) for everything else.

## 3. Modules / `import`

`prelude.lox` (about 60 lines) is copied into 11 of the 13 examples, and
`sync_prelude.py` keeps the copies in sync.

- **Options:** `import "prelude.lox";` that runs a file in the global scope
  (simplest), or real modules with their own namespace.

## 4. 256-constant limit per chunk, and constants are not deduplicated

`identifierConstant` calls `makeConstant` every time a name appears, so
every method name, global read/write and string literal in a chunk takes a
new slot, even if the same name is already in the table.

- **Hit in:** `02_symbolic` ("Too many constants in one chunk" at the top
  level) and `12_stack_vm` (the assembler class has ~20 methods).
- **Workaround:** wrap the driver code in `fun main() { ... } main();`. In
  `12_stack_vm`, declare the class inside a function that returns it, so its
  method names go into that function's chunk.
- **Fix:** deduplicate identifier constants (look up the string in a per-chunk
  table before adding), and/or add `OP_CONSTANT_LONG` / `OP_GET_GLOBAL_LONG`
  with 24-bit operands.

## 5. Call depth limit of 64 frames

`FRAME_MAX` is 64, so any recursion deeper than about 60 overflows.

- **Hit in:** `07_functional` (memoized `fib` uses two frames per level, so
  only `fib(25)`); `05_bst` (a 50-node degenerate tree is about the limit).
  Every linked-list function is written as a loop instead of recursion for
  this reason.
- **Options:** raise `FRAME_MAX` (e.g. 1024); grow the frame and value stacks
  dynamically; tail-call optimization for `return f(...)`.

## 6. Arrays / lists

There is no indexed collection. Every example uses hand-built cons lists, and
indexing is O(n) through `nth()`.

- **Hit in:** nearly all of them. `10_life` wires cells into a neighbor
  graph to avoid O(n) grid lookups. `12_stack_vm` resolves jump targets to
  list nodes because it can't index instructions.
- **Options:** a native `List`/array object with `[]` indexing, `push`,
  `pop`, `len`.

## 7. Maps / dictionaries

Clox already has a hash table (`table.c`) but Lox code can't use one
directly, except as an instance's fields with fixed names.

- **Hit in:** `07_functional` (memoize cache), `11_events` (topic → handlers),
  `12_stack_vm` (variables, label table). All are O(n) association lists.
- **Options:** a native `Map` class built on `Table`, or computed property
  access on instances (`obj[name]`, `getField(obj, name)`, `hasField`).

## 8. Anonymous functions (lambdas)

Every function value needs a named `fun` declaration first.

- **Hit in:** `07_functional` and `08_church` above all. `fun withF(f) { fun withX(x) {...}
  return withX; } return withF;` would be `fun (f) fun (x) ...`.
- **Options:** `fun (x) { ... }` as an expression (the Crafting Interpreters
  challenge), maybe with a short arrow form.

## 9. Fresh loop variable per iteration

A `for` loop has one variable for the whole loop, so closures created in
the body all see its final value.

- **Hit in:** `06_closures` shows it (`3 3 3`) and the workaround of copying
  into a body-local `var j = i;`.
- **Fix:** like JavaScript's `let` in `for`, give each iteration its own copy
  of the loop variable, and close the upvalue at the end of each iteration.

## 10. String operations

There's no length, indexing, substring, comparison (`"a" < "b"` is an error)
or char code.

- **Hit in:** `10_life` patterns are lists of booleans instead of strings
  like `".#."`. `12_stack_vm` builds programs with a fluent API because it
  can't tokenize source text. `05_bst` can only hold numbers.
- **Options:** `len(s)`, `s[i]` or `charAt`, `substr`, `ord`/`chr`, and
  lexicographic `<` / `>` on strings.

## 11. `%` operator and math natives

There's no modulo, `floor`, `sqrt` or `abs`.

- **Hit in:** `mod`/`floor` in the prelude (used by sorting's PRNG, life's
  torus wrapping, the stack VM's `mod` instruction); `sqrt` via Newton's
  method in `01_shapes`.
- **Options:** a `%` operator (`fmod`) and `floor`, `ceil`, `sqrt`, `abs`,
  `pow` natives.

## 12. Type introspection

There's no `instanceof`, `typeof` or `is` check.

- **Hit in:** `02_symbolic` needs to know if a node is a `Num` during
  simplification; it uses `isNum()` / `isZero()` methods with defaults on
  the base class.
- **Options:** `x is Num` (walking the superclass chain), or a `type(x)`
  native.

## 13. Optional and default parameters

Arity must match exactly.

- **Hit in:** `06_closures` message-passing account: `acct("balance", nil)`
  must pass a dummy amount.
- **Options:** default values (`fun f(a, b = nil)`) or treating missing
  arguments as `nil`.

## 14. Compound assignment and increment

`i = i + 1` appears in nearly every loop of every example.

- **Options:** `+=`, `-=`, `*=`, `/=` (and maybe `++`/`--`), all desugared
  in the compiler.

## 15. `break` / `continue`

- **Hit in:** `04_nqueens` symmetry check keeps looping after it already
  knows the answer. Several loops fold early exits into the loop condition.

## 16. Ternary / conditional expression

The `cond and "yes" or "no"` idiom works only when the middle value is
truthy. It is used throughout for yes/no output.

- **Options:** `cond ? a : b`, or `if` as an expression.

## 17. Print without a newline / print several values

- **Hit in:** `10_life` and `13_mandelbrot` build each row as a string
  first; `12_stack_vm`'s `emit` instruction buffers output until `cr`.
- **Options:** a `write()` native with no newline, or `print a, b, c;`.

## 18. Static methods / class-level fields

Classes can't have fields (`Expr.isMulByConst = nil` is an error) or static
methods.

- **Options:** `class` methods (the Crafting Interpreters metaclass
  challenge), or allow fields on class objects.
