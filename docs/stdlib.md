# Standard Library Reference

The standard library is written in GLisp and lives in `std/`. It is loaded
automatically at startup unless `--no-stdlib` is given. `std/std.lisp` loads the
modules in this order:

```
core.lisp -> math.lisp -> list.lisp -> iter.lisp -> types.lisp -> io.lisp
```

All definitions are installed in the global environment. Because `types.lisp`
loads before `io.lisp`, predicates such as `int?` and `file?` are available to
the I/O helpers.

## core.lisp

| Signature | Description |
| :--- | :--- |
| `(setq name value)` | Alias for `set`. |
| `(set! name value)` | Alias for `set`. |
| `(not x)` | `t` when `x` is nil, otherwise `nil`. |
| `(zero? x)` | True when `x = 0`. |
| `(pos? x)` | True when `x > 0`. |
| `(neg? x)` | True when `x < 0`. |
| `(identity x)` | Returns `x`. |
| `(id x)` | Returns `x`. |

## math.lisp

### Constants

| Name | Value |
| :--- | :--- |
| `PI` | 3.141592653589793 |
| `E` | 2.718281828459045 |
| `PHI` | 1.618033988749895 |
| `float=` | Alias for `approx=` |

### General numeric helpers

| Signature | Description |
| :--- | :--- |
| `(1+ x)` | `x + 1` |
| `(1- x)` | `x - 1` |
| `(even? x)` | Integer parity test |
| `(odd? x)` | Integer parity test |
| `(abs x)` | Absolute value |
| `(min a b)` | Smaller of two numbers |
| `(max a b)` | Larger of two numbers |
| `(clamp x low high)` | Clamps `x` to `[low, high]` |
| `(signum x)` | `-1`, `0`, or `1` |
| `(frac? x)` | True for floats with a fractional part; `nil` for non-floats |
| `(natural? x)` | True for non-negative numbers without a fractional part |
| `(approx= a b [rel-tol] [abs-tol])` | Approximate numeric equality (defaults: `1e-7`, `1e-9`) |
| `(trunc x)` | Truncates toward zero and returns a float |
| `(factorial x)` | Factorial of a non-negative integer (recursive) |
| `(gcd a b)` | Greatest common divisor (integer) |
| `(lcm a b)` | Least common multiple (integer) |
| `(hypot x y)` | `sqrt(x^2 + y^2)` with overflow-conscious scaling |
| `(lerp a b t)` | Linear interpolation between `a` and `b` |
| `(rem a b)` | Remainder consistent with truncation toward zero |
| `(mod a b)` | Floored modulo; the result follows the divisor's sign |
| `(deg->rad deg)` | Degrees to radians |
| `(rad->deg rad)` | Radians to degrees |

`std/math.lisp` defines `mod` as a Lisp function using `floor`. The native `%`
builtin uses C remainder semantics, where the sign follows the dividend.

### Powers and roots

| Signature | Description |
| :--- | :--- |
| `(cbrt x)` | Cube root |

`sqrt` and `pow` are native builtins; see [builtins.md](builtins.md#math).

### Trigonometry

| Signature | Description |
| :--- | :--- |
| `(tan x)`, `(cot x)`, `(sec x)`, `(csc x)` | Tangent, cotangent, secant, cosecant |
| `(asin x)`, `(acos x)`, `(atan x)`, `(acot x)` | Inverse trigonometric functions |
| `(asec x)`, `(acsc x)` | Inverse secant and cosecant |

### Hyperbolic functions

| Signature | Description |
| :--- | :--- |
| `(sinh x)`, `(cosh x)`, `(tanh x)` | Hyperbolic functions |
| `(coth x)`, `(sech x)`, `(csch x)` | Reciprocal hyperbolic functions |
| `(asinh x)`, `(acosh x)`, `(atanh x)` | Inverse hyperbolic functions |
| `(acoth x)`, `(asech x)`, `(acsch x)` | Inverse reciprocal hyperbolic functions |

Domain errors are reported with `assert` and abort the current form.

## list.lisp

| Signature | Description |
| :--- | :--- |
| `(list ...)` | Returns its arguments as a list. |
| `(first x)` | `car` |
| `(last x)` | Last element; O(n) |
| `(caar x)`, `(cadr x)`, `(cdar x)`, `(cddr x)`, `(caddr x)` | Conventional accessor compositions |
| `(reverse lst)` | Reverses a list (iterative) |
| `(append a b)` | Concatenates two lists (recursive) |
| `(member item lst)` | Returns the sublist starting at `item`, or `nil`; compares with `=` |

## iter.lisp

| Signature | Description |
| :--- | :--- |
| `(to-list coll)` | Converts a collection to a list. |
| `(to-array coll)` | Converts a collection to an array. |
| `(to-vector coll)` | Converts a collection to a vector. |
| `(map f coll)` | Applies `f` to each element, preserving the collection type. |
| `(filter pred coll)` | Keeps elements for which `pred` is true, preserving the collection type. |
| `(reduce f coll init)` | Left fold: `(f (f init x1) x2) ...` |
| `(each f coll)` | Calls `f` for each element; returns `nil`. |
| `(any? pred coll)` | True when at least one element satisfies `pred`. |
| `(all? pred coll)` | True when every element satisfies `pred`. |
| `(sort coll)` | Returns a sorted copy of a list, array or vector, in ascending order. |

`map` and `filter` return `nil` for an unknown collection type. `reduce`,
`each`, `any?`, and `all?` support lists, arrays, and vectors.

```lisp
(map 1+ [1 2 3])              ; => [2 3 4]
(filter even? '(1 2 3 4))     ; => (2 4)
(reduce + {1 2 3 4} 0)        ; => 10
(any? even? [1 3 5 6])        ; => t
(all? odd? [1 3 5])           ; => t
```

## types.lisp

| Signature | Description |
| :--- | :--- |
| `(typeid? x)` | True when `x` is a typeid. |
| `(instanceof? type x)` | True when `x` has the given typeid. |
| `(int? x)`, `(float? x)`, `(number? x)` | Numeric predicates |
| `(string? x)`, `(symbol? x)`, `(nil? x)` | Scalar predicates |
| `(null? x)` | Alias for `nil?` |
| `(function? x)` | True for functions, builtins, and special forms |
| `(list? x)`, `(array? x)`, `(vector? x)` | Collection predicates |
| `(collection? x)` | True for lists, arrays, and vectors |
| `(file? x)` | True for file handles |
| `(type= a b)` | True when `a` and `b` have the same typeid |

## io.lisp

### Output

| Signature | Description |
| :--- | :--- |
| `(write value [file])` | Writes the textual representation of `value` (default `*stdout*`). |
| `(write-each coll [file])` | Writes every element of `coll`. |
| `(print ...)` | Writes all arguments to `*stdout*` without a newline. |
| `(println ...)` | Writes all arguments to `*stdout*` followed by a newline. |
| `(eprintln ...)` | Writes each argument on its own line. The `e` prefix means "each". |
| `(fprint file ...)` | Writes arguments to `file`. |
| `(fprintln file ...)` | Writes arguments and a newline to `file`. |
| `(feprintln file ...)` | Writes each argument on its own line to `file`. |
| `(print-err ...)` | Writes arguments to `*stderr*`. |
| `(println-err ...)` | Writes arguments and a newline to `*stderr*`. |
| `(eprintln-err ...)` | Writes each argument on its own line to `*stderr*`. |

`write` uses `to-string`, which prints strings verbatim without quotes.

### Input

| Signature | Description |
| :--- | :--- |
| `(input [prompt])` | Writes `prompt`, reads a line from `*stdin*`, and returns it (or `nil` at EOF). |

### Files

| Signature | Description |
| :--- | :--- |
| `(closed? file)` | True when `file` is not open. |
| `(file-mode? file mode)` | True when **all** bits of `mode` are set. |
| `(readable? file)` | True when `file` has `file-read`. |
| `(writable? file)` | True when `file` has `file-write` or `file-append`. |
| `(appendable? file)` | True when `file` has `file-append`. |
| `(open-read path)` | Opens `path` with mode `file-read` (`"r"`). |
| `(open-write path)` | Opens `path` with mode `file-write` (`"w"`, truncates). |
| `(open-append path)` | Opens `path` with mode `file-append` (`"a"`). |
| `(open-read-write path)` | Opens `path` with mode `file-read | file-write` (`"r+"`, no truncation). |

### File modes

```lisp
(defconst file-read 1)
(defconst file-write 2)
(defconst file-append 4)
```

Modes are integers and can be combined with `logior` and inspected with
`logand`:

```lisp
(set f (open "data.txt" (logior file-read file-write)))
(file-mode f)       ; => 3
(readable? f)       ; => t
(writable? f)       ; => t
(appendable? f)     ; => nil
(close f)
```

See [builtins.md](builtins.md#io) for the full `open` mode rules.
