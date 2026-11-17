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
| `` `expr `` | quasiquote: `(quasiquote expr)` |
| `,expr` | unquote: `(unquote expr)` |
| `,@expr` | unquote-splicing: `(unquote-splicing expr)` |
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

Quasiquote is a template form: outside an `unquote`, symbols and lists are
taken as data; `,expr` evaluates `expr` and inserts the resulting value, while
`,@expr` evaluates `expr` and splices the elements of a list, array, vector, or
`nil`. Nested quasiquotes increase the quoting depth, so only unquotes at the
matching depth are evaluated. Quasiquote produces runtime data, which makes it
the natural tool for constructing code in macros.

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
| hashmap | `hashmap-t` | `(hash)` | string keys, values of any type; identity equality |
| function | `function-t` | `(lambda (x) x)` | closure |
| macro | `macro-t` | result of `defmacro` | expands in call position; not `apply`-able |
| builtin | — | `+`, `car` | implemented in C |
| special form | — | `if`, `let` | implemented in C |
| typeid | `typeid-t` | `int-t`, `file-t` | result of `typeof` |
| file | `file-t` | `*stdout*` | I/O handle |

`typeof` returns a typeid, which can be compared with `=` or tested with the
predicates from `std/types.lisp` (`int?`, `string?`, `list?`, ...).

Floats print in their shortest form with fifteen significant digits: `1.5`,
`3.14`, `0.3`. An integral float keeps its type visible as `1.0`, and a value
large enough for exponent notation prints as `1e+17`, which the reader cannot
lex back.

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

Functions and variables live in separate tables, but call resolution uses the
nearest lexical binding. Frames are searched from innermost to outermost; within
one frame a function binding wins over a variable binding. A local variable can
therefore hold a function and shadow a global function of the same name.

## Scoping and binding

GLisp uses lexical scope with chained environment frames.

```lisp
(set x 10)          ; nearest binding, or a new binding in the current frame
(set-global y 20)   ; always the global frame
(defvar z 30)       ; global, but only when z is not bound yet

(let ((a 1) (b 2)) (+ a b))          ; parallel bindings
(let* ((a 5) (b (* a 2))) (+ a b))   ; sequential bindings
```

- `let` evaluates initializers in the enclosing scope (parallel semantics).
- `let*` evaluates each initializer with the previous bindings visible.
- `set` updates the nearest existing variable binding; if the name is unbound,
  it defines the variable in the current frame.
- `defvar` declares a global variable. If the global name is already bound, its
  value and the init form are left untouched; otherwise the init form is
  evaluated in the current environment and stored globally. This makes `defvar`
  safe to re-evaluate when a file is loaded again.
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
- Recursion is supported. A call in tail position, meaning the last form of a
  body or of an `if`, `cond`, `when`, `unless` or `progn` branch, reuses the
  caller's frame, so a loop written that way runs in constant stack.

A function body is a sequence of expressions; the value of the last one is
returned. `defun` returns the newly created function, `lambda` returns an
anonymous closure.

Documentation strings are ordinary string expressions in a body; they are
evaluated for their side effects (none) and are not stored.

## Macros

```lisp
(defmacro name (parameters ...) body ...)
```

A macro is a function from **unevaluated forms** to a form. In call position the
evaluator does not evaluate the arguments: it converts them to runtime data,
runs the macro body, converts the returned data back to syntax, and evaluates
that expansion in the caller's environment.

```lisp
(defmacro my-unless (test &rest body)
  `(if ,test nil (progn ,@body)))

(my-unless nil (println "no") 42) ; => 42, println is not evaluated
```

- Macro expansion happens at evaluation time, so a macro must be defined before
  the call is evaluated. Macros defined in an inner `let`/function frame are
  visible to later forms in that frame and disappear with it.
- Macros live in the same name space as functions and special forms: a
  `defmacro` and a `defun` with the same name replace each other.
- `(macroexpand-1 form)` expands one outermost macro call; `(macroexpand form)`
  repeats until the head is no longer a macro. Both take and return runtime
  data and do not evaluate the expansion.
- `(macro? value)` is true for macro values, `(function? value)` is false, and
  `(typeof value)` is `macro-t`. Macros cannot be passed to `apply` or used as
  callbacks.
- Macros are non-hygienic: use `(gensym)` for bindings introduced by an
  expansion.
- Runaway expansion is stopped by an expansion-depth limit and reported as a
  recursion error.

## Control flow

`when`, `unless`, `cond`, `case`, `and`, `or`, and `dotimes` are macros provided
by `std/core.lisp`; they require the standard library (`--no-stdlib` omits
them). `if`, `while`, and `for` are part of the bare runtime.

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

A failed name lookup adds a suggestion when a bound name is close enough:
`undefined function 'printn' (did you mean 'print'?)`. The search covers the
variable and function namespaces of every enclosing frame and uses an edit
distance of two, tightened to one for names of three characters or fewer.

```lisp
(assert (= (+ 2 2) 4) "arithmetic broke")
(assert fact "message")
```

`assert` reports an assertion failure and aborts the current form when the
condition is `nil`. There is no `try`/`catch` or `unwind-protect`; a recoverable
error API is not implemented yet.

## Known limitations

- No `\uXXXX` escape; write a double quote as `\x22` because the tokenizer
  ends a string at the first raw `"`. Escapes produce single bytes, not code
  points.
- Macros are not hygienic; use `gensym` for introduced bindings.
- Tail calls are eliminated only where the call is the last form of a function
  body or of an `if`, `cond`, `when`, `unless` or `progn` branch. A call inside
  `let`, `let*`, `and`, `or`, `while` or `for` still consumes the C stack, and a
  self-recursive call there reaches the depth limit instead of looping forever.
- No module system; `load` evaluates into the global environment.
- No exception handling.
- `len` and `get` are linear for lists.
- Collections are mutable by reference through `set-at`, `push`, and `pop`.
