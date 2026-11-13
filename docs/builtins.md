# Builtin Reference

This reference covers the special forms and native builtins implemented in
`src/runtime/specforms.c` and `src/runtime/builtins.c`. Higher-level functions
written in Lisp are documented in [stdlib.md](stdlib.md).

- Special forms receive unevaluated syntax tree nodes.
- Builtins receive already-evaluated argument values.
- Unless stated otherwise, arguments are evaluated left to right.
- On error a builtin prints a diagnostic and returns `NULL`, aborting the
  current form.

## Special forms

### `if`

```
(if test consequent)
(if test consequent alternate ...)
```

Evaluates `test`; evaluates `consequent` when it is true, otherwise evaluates the
alternate expressions in order. With no alternate the result is `nil`.

```lisp
(if (> 3 2) "yes" "no") ; => "yes"
```

### `cond`

```
(cond (test body ...) ... (else body ...))
```

Tests clauses in order. The first true clause is selected and its body is
evaluated. A clause with an empty body returns the test value. `else` is a
constant that is always true. Returns `nil` when nothing matches.

### `and` / `or`

```
(and expr ...)
(or expr ...)
```

Short-circuiting boolean forms. `and` returns `t` when every expression is true
and `nil` otherwise. `or` returns `t` when any expression is true and `nil`
otherwise. Both return booleans, not the operand values.

### `while`

```
(while test body ...)
```

Repeats `body` while `test` is true. Returns `nil`.

### `for`

```
(for (variable collection) body ...)
```

Iterates over a list, array, vector, nil, or string. Strings yield
one-character strings. The variable is bound in a fresh lexical frame to a copy
of each element. Returns `nil`.

### `progn`

```
(progn expr ...)
```

Evaluates the expressions in order and returns the last value.

### `let` / `let*`

```
(let ((name init) ...) body ...)
(let* ((name init) ...) body ...)
```

Creates a new lexical frame. `let` evaluates all initializers in the enclosing
scope (parallel binding); `let*` evaluates each initializer with previous
bindings visible. A binding without an initializer is bound to `nil`.

### `set` / `set-global`

```
(set name value)
(set-global name value)
```

`set` updates the nearest existing variable binding, or defines `name` in the
current frame. `set-global` always writes to the global frame. Both return the
assigned value. Constants cannot be modified.

### `defun` / `lambda`

```
(defun name (parameters ...) body ...)
(lambda (parameters ...) body ...)
```

Creates a function. Parameters may be required, `&optional` (with optional
defaults), or `&rest`. `defun` binds the function in the current environment and
returns it; `lambda` returns an anonymous closure without binding it.

```lisp
(defun greet (who &optional (greeting "Hello") &rest extra)
  (list greeting who extra))
```

### `defconst`

```
(defconst name value [docstring])
(defconstant name value [docstring])   ; alias
```

Defines an immutable global constant. Re-defining an existing constant with an
equal value is allowed. Built-in constants (`t`, `nil`, `else`, typeids) are
protected.

### `quote`

```
(quote expr)
'expr
```

Returns `expr` as data. Symbols become symbol values; lists become lists.

### `function`

```
(function name)
#'name
```

Returns the callable bound to `name` in the function namespace.

### `add-to-list`

```
(add-to-list variable element [append])
```

Adds `element` to the list stored in `variable` if it is not already present
(compared with `=`). By default the element is prepended; a true third argument
appends it to the end. Accepts a quoted symbol as the variable. Returns the
resulting list.

### `pragma`

```
(pragma name argument ...)
```

Dispatches a runtime directive.

| Directive | Arguments | Result |
| :--- | :--- | :--- |
| `gc-allocated` | none | number of live GC values |
| `gc-collect` | none | `nil`; forces a collection |
| `gc-toggle` | optional boolean | new state; toggles or sets automatic GC |
| `gc-enabled?` / `gc-enabled` | none | current automatic GC state |

## Native builtins

### Runtime

| Signature | Description |
| :--- | :--- |
| `(exit [code])` | Terminates the process with `code` (default `0`). |
| `(typeof value)` | Returns the value's typeid. |
| `(load path)` | Evaluates a source file relative to the current unit and returns its last value. |
| `(assert condition [message])` | Returns `nil`; aborts the current form with a diagnostic if `condition` is nil. |

### Arithmetic

| Signature | Description |
| :--- | :--- |
| `(+ ...)` | Sum; `(+)` is `0`. Promotes to float if any argument is a float. |
| `(- x ...)` | Subtraction. With one argument, negation. |
| `(* ...)` | Product; `(*)` is `1`. Promotes to float if any argument is a float. |
| `(/ x ...)` | Left-associative division. Integer division truncates toward zero. Division by integer zero is an error. |
| `(% a b)` | Integer remainder (C semantics; sign follows the dividend). |
| `(int x)` | Converts a number to int, truncating toward zero. Returns ints unchanged. |
| `(float x)` | Converts a number to float. Returns floats unchanged. |

### Math

All of these return floats.

| Signature | Description |
| :--- | :--- |
| `(sqrt x)` | Square root |
| `(pow x y)` | `x` raised to `y`; `(pow 0 0)` is an error |
| `(fma a b c)` | Fused multiply-add: `a*b + c` |
| `(copysign magnitude sign)` | Magnitude with the sign of `sign` |
| `(exp x)`, `(exp2 x)` | `e^x`, `2^x` |
| `(log x)`, `(log2 x)` | Natural and base-2 logarithm |
| `(floor x)`, `(ceil x)`, `(round x)` | Rounding |
| `(sin x)`, `(cos x)` | Trigonometric functions |
| `(atan2 y x)` | Two-argument arctangent |

### Comparison

| Signature | Description |
| :--- | :--- |
| `(= x y ...)` / `(eq x y ...)` | Chain comparison using structural equality. |
| `(> x y ...)`, `(< x y ...)`, `(>= x y ...)`, `(<= x y ...)` | Chain comparison. |

`=` is deep equality: numbers compare numerically (int/float may be mixed),
symbols and strings by content, and lists/arrays/vectors element-wise.
Functions compare by name (or identity for anonymous functions). A single
argument is always equal to itself.

### Pairs and lists

| Signature | Description |
| :--- | :--- |
| `(car list)` | First element of a non-empty list. |
| `(cdr list)` | Tail of a non-empty list. |
| `(cons a b)` | New cons cell. |
| `(list ...)` | See [stdlib](stdlib.md#listlisp); implemented in Lisp. |
| `(len sequence)` | Length. O(1) for arrays, vectors, strings; O(n) for lists. `nil` has length 0. |
| `(get sequence index)` or `(get index sequence)` | Element access. Also `nth` and `aref`. O(1) for arrays/vectors/strings, O(n) for lists. Strings yield one-character strings. |

### Arrays and vectors

| Signature | Description |
| :--- | :--- |
| `(array ...)` | Builds a fixed array from its arguments. |
| `(vector ...)` | Builds a dynamic vector from its arguments. |
| `(set-at collection index value)` / `(aset ...)` | Replaces an element of an array or vector and returns the new value. |
| `(push vector value ...)` / `(vector-push ...)` | Appends values and returns the vector. |
| `(pop vector)` / `(vector-pop vector)` | Removes and returns the last element; an empty vector is an error. |

### Functions

| Signature | Description |
| :--- | :--- |
| `(apply function arg ... collection)` | Calls `function` with the leading arguments followed by the elements of `collection` (list, array, vector, or nil). |

```lisp
(apply + 1 2 '(3 4)) ; => 10
```

### Strings and printing

| Signature | Description |
| :--- | :--- |
| `(to-string value)` | Returns a string representation. Strings are returned as-is. |
| `(write-string string [file])` | Writes bytes to `file` (default `*stdout*`) and returns the number written. |
| `(string-append string ...)` | Returns a new string with every argument concatenated. |
| `(substring string start [end])` | Returns the bytes in `[start, end)`; `end` defaults to the string length. Raises when the range is out of bounds. |
| `(string-upcase string)` | Returns a copy with ASCII letters uppercased. |
| `(string-downcase string)` | Returns a copy with ASCII letters lowercased. |

### I/O

| Signature | Description |
| :--- | :--- |
| `(open path mode)` | Opens a file. `mode` is a string or an integer bitmask. |
| `(close file)` | Closes a file; returns `t` on success and `nil` for standard streams, already-closed files, or failure. |
| `(open? file)` | True when the file is open. |
| `(eof? file)` | True when the end-of-file indicator is set; closed files are an error. |
| `(file-mode file)` | Returns the mode bitmask. Survives `close`. |
| `(flush [file])` | Flushes buffered output (default `*stdout*`). |
| `(read-line [file])` | Reads one line, strips `\n`/`\r\n`, and returns it; returns `nil` at EOF. |

#### Mode strings

`open` accepts `"r"`, `"w"`, or `"a"`, optionally combined with `+` and/or `b`
(e.g. `"r+"`, `"wb"`, `"a+"`). `w` truncates; `a` appends.

#### Mode bitmask

| Bit | Constant (stdlib) | Meaning |
| :--- | :--- | :--- |
| `1` | `file-read` | Readable |
| `2` | `file-write` | Writable (truncates unless combined with append) |
| `4` | `file-append` | Appends at end of file |

Integer modes are combined with `logior`:

```lisp
(open "log.txt" (logior file-append file-read)) ; "a+"
```

Integer-to-`fopen` mapping: `1 -> "r"`, `2 -> "w"`, `4 -> "a"`,
`3 -> "r+"`, `5 -> "a+"`, `6 -> "a"`, `7 -> "a+"`. Zero, negative values, and
unknown bits are rejected.

### Bitwise operations

| Signature | Description |
| :--- | :--- |
| `(logand a b)` | Bitwise AND |
| `(logior a b)` | Bitwise OR |
| `(logxor a b)` | Bitwise XOR |
| `(lognot a)` | Bitwise NOT |

All bitwise builtins require int arguments.
