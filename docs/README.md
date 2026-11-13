# GLisp Documentation

GLisp is a small Lisp dialect implemented in C17. This directory contains the
language reference, the native builtin reference, the standard library
reference, and the C API documentation.

| Document | Contents |
| :--- | :--- |
| [language.md](language.md) | Reader syntax, values, scoping, evaluation, error model |
| [builtins.md](builtins.md) | Special forms and native (C) builtins |
| [stdlib.md](stdlib.md) | Functions provided by `std/*.lisp` |
| [c-api.md](c-api.md) | Internal C modules, ownership rules, GC contract |

## Documentation conventions

- `(name arg ...)` denotes a required argument.
- `[arg]` denotes an optional argument.
- `...` denotes zero or more trailing arguments.
- Return values are written as `=> value`.
- "Raises" means that evaluation of the current form is aborted: the builtin
  reports a diagnostic and returns `NULL`, which propagates to the caller.

## See also

- [README](../README.md) — project overview and build instructions
- [LICENSE](../LICENSE) — GNU General Public License v3.0
