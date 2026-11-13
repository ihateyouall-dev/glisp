# C API Reference

This document describes the internal C modules of GLisp. The API is intended
for contributors and for programs that embed or extend the interpreter; it is
not a stable ABI.

## Layering

```
utils (cons, array, vector, hashmap, ansi)
  |
  +-- lexer -> parser -> ast
  |                        |
  +-- diagnostics <--------+
  |
  +-- value -> env -> callstack -> eval
        |                          |
        +-- memory (GC) <----------+
        |
        +-- builtins
        +-- specforms
        +-- pragma
```

`main.c` wires the pipeline together: initialize the global environment, load
the standard library, then either evaluate a file or run the REPL.

## Conventions

- **Errors.** Functions that can fail return `NULL` and report a diagnostic via
  `gl_diagnostic_report_error`. There is no error object returned to the caller.
- **Ownership.** Runtime values (`gl_value_t`) are owned by the garbage
  collector. Constructors (`gl_value_make_*`) allocate and register a value.
  AST nodes (`gl_ast_node_t`) are owned by their creator and freed with
  `gl_ast_destroy`.
- **Strings.** Functions taking strings generally treat them as borrowed,
  NUL-terminated buffers unless documented otherwise.
- **Threads.** The runtime is single-threaded; the global heap, environment, and
  call stack are process-wide globals.

## Utility headers (`src/utils/`)

| Header | Contents |
| :--- | :--- |
| `location.h` | `gl_location_t`: byte offset, line, column, source unit name, and source buffer. |
| `cons.h` | `CONS_DECLARE(T, Name)` / `CONS_DEFINE(T, Name)`: typed cons cell. |
| `array.h` | `ARRAY_DECLARE(T, Name)` / `ARRAY_DEFINE(T, Name)`: fixed-size typed array. |
| `vector.h` | `VECTOR_DECLARE(T, Name)` / `VECTOR_DEFINE(T, Name)`: growable typed vector. |
| `hashmap.h` | `HASHMAP_DECLARE(T, Name)` / `HASHMAP_DEFINE(T, Name)`: chained hash map keyed by C strings. |
| `ansi.h` | ANSI SGR escape sequences and `gl_process_ansi_str` helpers used by diagnostics. |
| `strdup.h` | `strdup` shim for MSVC. |
| `rl_wrapper.h` | Convenience include for GNU Readline. |

The container macros generate a `Name_t` struct plus `Name_init`, `Name_destroy`,
`Name_push_back`, `Name_at`, and related functions. Exactly one translation
unit must instantiate a given `Name`. None of the generated containers own
their elements unless a `destroy_fn` is supplied to `Name_init`.

## Front end

### `lexer.h`

- `gl_lex_token_type_t` enumerates token kinds: parentheses, quote/funquote,
  numbers, symbols, strings, and EOF.
- `gl_lexer_t` holds the source length and current `gl_location_t`.
- `gl_make_lexer(src, unit_name)` / `gl_lexer_init(...)` create a lexer.
- `gl_lexer_next(lexer)` returns the next token; `gl_lexer_tokenize(lexer)`
  returns a vector of all tokens.
- `gl_lex_token_destroy(token)` releases a token's text buffer.

Tokens carry a `gl_lex_token_status_t`; `LEX_ERROR` indicates malformed input
such as an unterminated string.

### `parser.h`

- `gl_make_parser(src, name)` tokenizes a source unit.
- `gl_parser_parse(parser)` parses and returns one top-level AST node, or
  `NULL` at the end/on error.
- `gl_parser_destroy(parser)` frees the parser and its tokens.

### `ast.h`

`gl_ast_node_t` is a tagged union over int, float, symbol, string, cons, array,
and vector nodes. Every node stores a `gl_location_t`.

- `gl_ast_make_*` constructors allocate a node.
- `gl_ast_copy(node)` performs a deep copy.
- `gl_ast_destroy(node)` frees a tree.
- `gl_ast_cons_nth_car` / `gl_ast_cons_nth_cdr` index into a cons list.
- `gl_ast_cons_length` counts list elements.
- `gl_ast_make_quote` / `gl_ast_make_funquote` build quote nodes.

### `diagnostics.h`

- `gl_make_error(type, severity, location, msg)` allocates an error; `msg` is
  borrowed.
- `gl_diagnostic_report_error(err)` prints a colored source snippet and frees
  the error.
- `gl_diagnostic_show_frame(loc)` prints a single source frame.
- `gl_diagnostic_report_arity_error` and its variadic/range variants report
  arity mismatches.

## Runtime

### `value.h`

`gl_value_t` is the universal runtime value:

```c
typedef struct gl_value_t {
    gl_value_type_t type;
    void *val;
    int gc_marked;
    struct gl_value_t *next_in_heap;
} gl_value_t;
```

- `GL_VAL_*` enumerates all value types.
- Constructors: `gl_value_make_int`, `_float`, `_symbol`, `_cons`,
  `_cons_raw`, `_string`, `_nil`, `_bool`, `_typeid`, `_file`, `_builtin`,
  `_specform`, `_function`, `_env`, `_array`, `_array_from`, `_vector`,
  `_vector_from`.
- Accessors validate the type with `assert` and return the payload:
  `gl_value_get_*`.
- `gl_value_copy(value)` deep-copies the value (lists are copied recursively).
- `gl_value_compare(a, b)` returns `0` when equal and a positive/negative value
  otherwise; it implements the language's `=` semantics, including mixed
  int/float comparison.
- `gl_value_fprint` / `gl_value_print` / `gl_value_to_string` render a value.
- `gl_value_get_bool` returns true for every value except nil.

`gl_function_t` describes a user function: AST body, parameter list, captured
closure, name, and arity bounds. `gl_function_param_t` models required,
optional, and rest parameters.

### `env.h`

`gl_env_t` is a lexical frame with three hash tables (`variables`, `functions`,
`constants`) and a `parent` pointer.

- `gl_make_env(parent)` allocates a frame.
- `gl_make_global_env()` builds the initial global frame with builtins,
  special forms, and constants.
- `gl_init_global_env()` wraps the global frame in a `GL_VAL_ENV` and stores it
  in `gl_global_env`.
- `gl_env_set_var` / `gl_env_set_fun` / `gl_env_set_const` insert bindings.
- `gl_env_get_var` / `gl_env_get_fun` search the frame chain.
- `gl_env_is_const` reports whether a name is protected.

### `callstack.h`

The call stack stores `gl_call_frame_t` entries (`func_name`, call AST node,
and lexical environment). It serves two purposes: resolving the current
environment and producing stack traces.

- `gl_call_stack_push_call(function, node)` allocates a child environment of the
  function's closure and pushes a frame.
- `gl_call_stack_push_env(env, node, name)` pushes an explicit frame (used by
  `let`, `let*`, and `for`).
- `gl_call_stack_pop_call` / `pop_env` remove the top frame.
- `gl_current_env()` returns the environment of the top frame or the global
  environment.
- `gl_call_stack_print_stacktrace()` prints and clears all frames.

### `eval.h`

- `gl_eval(node)` evaluates one AST node. It is the only safe point at which the
  collector may run automatically.
- `gl_parse_and_eval(parser, abort_on_error)` loops over top-level forms.
- `gl_eval_args(args)` evaluates a list of argument AST nodes into a
  `gl_function_args_t`. It pushes each evaluated value as a temporary GC root.
- `gl_call_function(loc, callable, argv)` invokes a callable with an existing
  argument array.

### `memory.h` (garbage collector)

The collector is mark-and-sweep over a global list of `gl_value_t` nodes.

- `gl_register_value(value)` links a newly allocated value into the heap.
- `gl_gc_collect()` forces a full collection.
- `gl_gc_maybe_collect()` collects when the heap exceeds the current
  threshold. The threshold starts at `GL_GC_THRESHOLD` and, after a collection,
  is raised to twice the live set so that a large but stable heap does not
  trigger a collection at every safe point. It is called at the start of
  `gl_eval`.
- `gl_gc_root_mark()` returns the current temporary-root stack depth.
- `gl_gc_push_root(value)` protects a value that is only reachable from a C
  local.
- `gl_gc_roots_release(mark)` pops roots back to a saved depth.
- `gl_register_source(buf, unit_name)` keeps source text alive for diagnostics.
- `gl_destroy_memory_heap()` frees all values and registered sources.

### `builtins.h` and `specforms.h`

A native function has the signature defined by `GL_BUILTIN(Name)`:

```c
gl_value_t *gl_builtin_name(gl_location_t called_at, gl_function_args_t *args);
```

`args` contains evaluated values with their source locations; use `args->size`
and `args->data[i]->val`.

A special form has the signature defined by `GL_SPECIAL_FORM(Name)`:

```c
gl_value_t *gl_specform_name(gl_env_t *env, gl_ast_node_t *args);
```

The AST is unevaluated; the special form decides what to evaluate.

### `pragma.h`

`GL_PRAGMA(Name)` declares a runtime directive with the same signature as a
builtin. `gl_dispatch_pragma` resolves a directive name to its implementation.

## Extending GLisp

### Adding a builtin

1. Declare it in `src/runtime/builtins.h` with `GL_BUILTIN(name);`.
2. Implement it in `src/runtime/builtins.c`:

```c
GL_BUILTIN(shout) {
    EXPECT_ARGS(1);
    gl_value_t *text = NTH_VAL(0);
    EXPECT_STRING(text);

    char buf[256];
    snprintf(buf, sizeof(buf), "%s!", gl_value_get_string(text));
    return gl_value_make_string(buf);
}
```

3. Register it in `src/runtime/env.c`:

```c
DEFINE_BUILTIN("shout", shout);
```

The `EXPECT_*`, `NTH_VAL`, and `ARGC` helpers are private to
`builtins.c`; replicate them (or validate manually) in a different translation
unit.

### Adding a special form

1. Declare it in `src/runtime/specforms.h` with `GL_SPECIAL_FORM(name);`.
2. Implement `gl_specform_name(gl_env_t *env, gl_ast_node_t *args)`.
3. Register it with `DEFINE_SPECFORM("name", name);`.

### Adding a pragma

1. Declare `GL_PRAGMA(name);` in `pragma.h`.
2. Implement it in `pragma.c`.
3. Add a branch in `gl_dispatch_pragma`.

### GC rules for extensions

The collector does not scan the C stack. Whenever a value created or received
inside a builtin must survive a call to `gl_eval`, `gl_call_function`, or any
other operation that may trigger collection, protect it:

```c
size_t mark = gl_gc_root_mark();
gl_gc_push_root(value);
/* ... code that may evaluate or allocate ... */
gl_gc_roots_release(mark);
```

Values stored in an environment, in a cons cell reachable from a root, or in an
array/vector reachable from a root do not need separate protection. The
interpreter itself roots function arguments, collection literals, `for`
iteration state, and similar transient values.

## Build integration

`src/CMakeLists.txt` exposes these libraries:

| Target | Sources |
| :--- | :--- |
| `lexer` | `lexer.c` (+ `vector`) |
| `ast` | `ast.c` |
| `parser` | `parser.c` (+ `lexer`, `ast`, `vector`, `diagnostics`) |
| `diagnostics` | `diagnostics.c` |
| `runtime` | every `runtime/*.c` (+ `parser`, `ast`) |
| `vector`, `hashmap` | header-only interface targets |

The `glisp` executable links `main.c` against `runtime` and GNU Readline.
