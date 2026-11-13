# The GLisp Language

This document describes the syntax and semantics of GLisp as implemented by the
current tree-walking interpreter.

## Source units

A source unit is a sequence of expressions. Expressions may span lines and are
evaluated in order; the value of a file is the value of its last expression.

```lisp
;;; Line comments start with a semicolon.
(print "hello")
```

- A `#!` line at the very beginning of a unit is skipped, which allows
  executable scripts.
- Only line comments exist. There is no block-comment syntax.
- Symbols are case-sensitive.

## Reader syntax

| Form | Meaning |
| :--- | :--- |
| `123` | 64-bit signed integer |
| `3.14` | long-double float (a decimal point is required) |
| `"text"` | string |
| `symbol` | symbol |
| `(a b c)` | list form / application |
| `[a b c]` | fixed-size array literal |
| `{a b c}` | dynamic vector literal |
| `'expr` | quote: `(quote expr)` |
| `#'name` | function quote: `(function name)` |
| `nil` | the empty list and the only false value |

Numbers are decimal. Scientific notation (`1e9`), hexadecimal, and digit
separators are not supported. A leading `-` is recognized only when followed by
a digit.

String literals are copied verbatim up to the next double quote; escapes are
processed afterwards, when the parser builds the string node. Supported
escapes are `\n`, `\r`, `\t`, `\a`, `\b`, `\f`, `\v`, `\\`, `\'`, `\?`, `\e`
and `\E` (0x1B), `\xHH` with one or two hex digits, and octal with one to three
digits (`\033`). An escape that is not listed keeps the character without the
backslash: `"\U"` is the one-character string `U`. A `\x` with no hex digit
after it is kept as `\x`. A trailing backslash is kept, so `"abc\"` is `abc\`.
The reader stops at the first `"`, so a double quote cannot be embedded and
`"\""` is a syntax error. A NUL byte from `\0` or `\x00` ends the string. The
printer emits strings verbatim, so `write` cannot produce a form that the reader
could read back.

The reader does not recognize dotted-pair syntax (`(a . b)`); improper lists can
only be built programmatically with `cons`.

There is no quasiquote (`\``, `,`, `,@`). A quote is a single token, not a
reader macro that composes with other forms.

## Values

| Type | Typeid | Examples | Notes |
| :--- | :--- | :--- | :--- |
| nil | `nil-t` | `nil`, `'()` | singleton; false |
| int | `int-t` | `42` | `int64_t` |
| float | `float-t` | `3.14` | `long double` |
| symbol | `symbol-t` | `foo` | compared by name |
| string | `string-t` | `"hi"` | immutable, NUL-terminated |
| list | `list-t` | `'(1 2 3)` | cons cells; may be improper |
| array | `array-t` | `[1 2 3]` | fixed size, O(1) index |
| vector | `vector-t` | `{1 2 3}` | growable, amortized O(1) push |
| function | `function-t` | `(lambda (x) x)` | closure |
| builtin | — | `+`, `car` | implemented in C |
| special form | — | `if`, `let` | implemented in C |
| typeid | `typeid-t` | `int-t`, `file-t` | result of `typeof` |
| file | `file-t` | `*stdout*` | I/O handle |

`typeof` returns a typeid, which can be compared with `=` or tested with the
predicates from `std/types.lisp` (`int?`, `string?`, `list?`, ...).

## Truthiness

Only `nil` is false. `t` and `else` are constants bound to true. Every other
value is true, including `0`, `0.0`, `""`, `[]`, and `{}`.

## Evaluation

Atoms evaluate as follows:

- integers, floats, strings, and `nil` evaluate to themselves;
- a symbol is looked up as a variable first, then as a function;
- an array or vector literal evaluates each element and builds a new collection.

A list evaluates as an application unless its head is a special form:

1. The head is resolved to a callable (a function, builtin, or special form).
   If the head is an expression, that expression is evaluated.
2. Arguments are evaluated left to right (special forms decide themselves which
   arguments to evaluate).
3. The callable is invoked.

A variable holding a function can be called directly:

```lisp
(set f (lambda (x) (* x x)))
(f 5) ; => 25
```

### Namespaces and lookup

Functions and variables live in separate tables. When the head of a form is a
symbol, GLisp searches the **function** namespace first and only then the
**variable** namespace. This means a global function shadows a local variable of
the same name in call position. Avoid naming local parameters after global
functions.

## Scoping and binding

GLisp uses lexical scope with chained environment frames.

```lisp
(set x 10)          ; nearest binding, or a new binding in the current frame
(set-global y 20)   ; always the global frame

(let ((a 1) (b 2)) (+ a b))          ; parallel bindings
(let* ((a 5) (b (* a 2))) (+ a b))   ; sequential bindings
```

- `let` evaluates initializers in the enclosing scope (parallel semantics).
- `let*` evaluates each initializer with the previous bindings visible.
- `set` updates the nearest existing variable binding; if the name is unbound,
  it defines the variable in the current frame.
- Functions capture the environment in which they are created, so closures see
  the bindings that were visible at definition time.

### Constants

`defconst` (alias `defconstant`) defines an immutable global constant:

```lisp
(defconst PI 3.14159265 "ratio of circumference to diameter")
```

`t`, `nil`, `else`, and all built-in typeids (`int-t`, `list-t`, ...) are
protected. Attempts to modify a constant with `set`, `set-global`,
`add-to-list`, or to shadow it in `let`, `let*`, or `for` are syntax errors.

## Functions

```lisp
(defun name (required &optional (opt default) &rest rest) body ...)
(lambda (required ...) body ...)
```

- Required parameters must be supplied.
- `&optional` parameters may have defaults. Default expressions are evaluated
  in the function's environment and may reference earlier parameters.
- `&rest` collects the remaining arguments into a list.
- Arity is checked at call time. Functions with `&rest` require at least their
  required arguments; others require an argument count within
  `[min, max]`.
- Parameters are bound to deep copies of the argument values.
- Recursion is supported, but calls are not tail-call optimized.

A function body is a sequence of expressions; the value of the last one is
returned. `defun` returns the newly created function, `lambda` returns an
anonymous closure.

There is no macro system. Documentation strings are ordinary string expressions
in a body; they are evaluated for their side effects (none) and are not stored.

## Control flow

### `if`

```lisp
(if test consequent)
(if test consequent alternate ...)
```

Only the selected branch is evaluated. Multiple alternate expressions are
evaluated in order and the last value is returned. If no alternate is present,
the result is `nil`.

### `cond`

```lisp
(cond
  ((< x 0) "negative")
  ((= x 0) "zero")
  (else    "positive"))
```

Clauses are tested in order. A clause whose test succeeds evaluates its body
and returns the last value; a clause with no body returns the test value. If no
clause matches, the result is `nil`.

### `and` / `or`

Both short-circuit and return `t` or `nil`, not the operand values:

```lisp
(and (> x 0) (< x 10)) ; => t or nil
(or (nil? x) (= x 0))  ; => t or nil
```

### `while`

```lisp
(while condition body ...)
```

Evaluates the body repeatedly while the condition is true; returns `nil`.

### `for`

```lisp
(for (item collection) body ...)
```

Iterates over a list, array, vector, or string (yielding one-character strings)
and returns `nil`. The loop variable is bound in a fresh lexical frame and gets
a copy of each element.

### `progn`

Evaluates its arguments in order and returns the last value.

## Collections

### Lists

```lisp
'(1 2 3)
(cons 1 '(2 3))
(car '(1 2 3))  ; => 1
(cdr '(1 2 3))  ; => (2 3)
```

Lists are cons cells; they may be improper. `len` and `get` are O(n) on lists.

### Arrays and vectors

```lisp
[1 2 3]        ; fixed-size array
{1 2 3}        ; growable vector

(get [10 20 30] 1)   ; => 20
(set-at [10 20 30] 1 99)
(push {1 2} 3 4)     ; => {1 2 3 4}
(pop {1 2 3})        ; => 3
```

Strings are immutable but indexable with `get`, which returns a one-character
string.

### Iteration helpers

`std/iter.lisp` provides `map`, `filter`, `reduce`, `each`, `any?`, and `all?`
over lists, arrays, and vectors. `map` and `filter` preserve the input
collection type.

## I/O

```lisp
(set f (open "notes.txt" "r"))
(read-line f)
(close f)

(set out (open "out.txt" (logior file-read file-write)))
(write-string "hello" out)
(flush out)
(close out)
```

`open` accepts either a mode string (`"r"`, `"w"`, `"a"`, optionally with `+`
and/or `b`) or an integer bitmask (`file-read`, `file-write`, `file-append`, see
[stdio reference](stdlib.md#file-modes)). Standard streams are bound to
`*stdin*`, `*stdout*`, and `*stderr*`.

## Garbage collection

Values are managed by a mark-and-sweep collector. Collection is triggered at
evaluation safe points once the heap exceeds its threshold. It can be inspected
and controlled from Lisp:

```lisp
(pragma gc-allocated) ; number of live GC nodes
(pragma gc-collect)   ; force a collection
(pragma gc-enabled?)  ; is automatic collection enabled?
(pragma gc-toggle)    ; toggle automatic collection
(pragma gc-toggle t)  ; enable explicitly
```

## Error model

There is no exception mechanism. Errors are reported as diagnostics and abort
the current form by propagating `NULL` up the evaluator. A diagnostic includes
the source location, a colored source snippet, and a call stack for function
calls.

```lisp
(assert (= (+ 2 2) 4) "arithmetic broke")
(assert fact "message")
```

`assert` reports an assertion failure and aborts the current form when the
condition is `nil`. There is no `try`/`catch` or `unwind-protect`; a recoverable
error API is not implemented yet.

## Known limitations

- No `\uXXXX` escape, and a literal `"` cannot be written. Escapes produce
  single bytes, not code points.
- No macros or quasiquote.
- No tail-call optimization; deep recursion consumes the C stack.
- No module system; `load` evaluates into the global environment.
- No exception handling.
- Function namespace takes precedence over variables in call position.
- `len` and `get` are linear for lists.
- Collections are mutable by reference through `set-at`, `push`, and `pop`.
