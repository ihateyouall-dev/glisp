# GLisp

GLisp is a lightweight Lisp dialect implemented in C17. It combines a small
tree-walking interpreter, a custom mark-and-sweep garbage collector, lexical
closures, and a Lisp-level standard library.

The project is under active development. The language and C API are not yet
frozen.

## Highlights

- **Lexical scoping and closures.** Functions capture their defining
  environment.
- **Flexible parameter lists.** Required arguments, `&optional` parameters with
  defaults, and variadic `&rest`.
- **Native collections.** Cons lists, fixed-size arrays (`[...]`), and growable
  vectors (`{...}`).
- **Mutable bindings and constants.** `set`, `set-global`, and immutable
  `defconst` values, including protected built-ins.
- **Control flow.** `if`, `cond`, short-circuiting `and`/`or`, `while`, and
  `for` over lists, arrays, vectors, and strings.
- **Garbage collection.** Mark-and-sweep with runtime inspection through
  `pragma`.
- **I/O.** Files, standard streams, line input, buffered output, and composable
  file-mode bitmasks.
- **Diagnostics.** Colored source snippets with line/column carets and call
  stack traces.
- **Readline REPL.** Interactive development with line editing and in-session
  history.
- **Autoloaded standard library.** Math, list utilities, higher-order
  functions, type predicates, and I/O helpers.

## Quick start

```bash
git clone https://github.com/ihateyouall-dev/glisp.git
cd glisp
make
./build/glisp
```

```lisp
GLisp REPL version 0

glisp> (defun square (x) (* x x))
(lambda (x) ...)
glisp> (map square [1 2 3 4 5])
[1 4 9 16 25]
glisp> (exit)
```

Run a script:

```bash
./build/glisp path/to/script.lisp
```

## Requirements

- A C17 compiler (Clang 15+ or GCC 12+)
- GNU Make
- CMake 3.24+
- Conan 2.x
- GNU Readline (installed by Conan)

On Windows, use the MSYS2 MinGW-w64 toolchain with the `readline` and
`pkgconf` packages. CMake falls back to `pkg-config` when no Conan-provided
`readline` package is present.

## Building

The `Makefile` installs Conan dependencies, configures a CMake preset, and
builds the interpreter.

```bash
make                     # Release build (default)
make BUILD_TYPE=Debug    # Debug build with AddressSanitizer and UBSan
make BUILD_TYPE=Test     # Debug build with tests enabled
make BUILD_TYPE=Leaks    # Debug build for the macOS `leaks` tool
make clean               # Remove the build directory
```

Builds are placed in `build/`. The executable is `build/glisp`.

On Windows (MSYS2), or when a Conan toolchain is not available, let CMake find
Readline through `pkg-config` instead:

```bash
make USE_CONAN=0 BUILD_TYPE=Test CMAKE_ARGS='-G Ninja'
```

If you prefer to drive CMake directly:

```bash
conan profile detect --force
conan install . --output-folder=build --build=missing -s compiler.cstd=17
cmake -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

```text
Usage: glisp [options] [file]

Options:
  --no-stdlib    Do not autoload the standard library
  -h, --help     Show help
```

- With a `file`, GLisp evaluates it and exits.
- Without a file, GLisp starts the interactive REPL.
- Scripts may start with a shebang:

```lisp
#!/usr/bin/env glisp
(println "Hello, GLisp!")
```

### Environment variables

| Variable | Description |
| :--- | :--- |
| `GLISP_STD_PATH` | Path to the standard library directory or `std.lisp`. |
| `GLISP_STD` | Fallback alias for `GLISP_STD_PATH`. |

If neither variable is set, GLisp looks for `./std/std.lisp` and then for the
compiled-in source directory.

## Language at a glance

```lisp
;; Variables and constants
(set counter 0)
(defconst LIMIT 10)

;; Functions with optional and rest parameters
(defun describe (x &optional (prefix "value") &rest tags)
  (list prefix x tags))

;; Control flow
(while (< counter LIMIT)
  (set counter (1+ counter)))

;; Collections
(set xs '(1 2 3))
(set arr [10 20 30])
(set vec {1 2 3})
(push vec 4)

;; Higher-order functions
(map 1+ [1 2 3])            ; => [2 3 4]
(filter even? '(1 2 3 4))   ; => (2 4)
(reduce + {1 2 3} 0)        ; => 6

;; File modes are bitmasks
(set out (open "log.txt" (logior file-write file-append)))
(write-string "entry\n" out)
(close out)
```

See the [language reference](docs/language.md) for the full syntax and
semantics.

## Standard library

`std/` is loaded automatically unless `--no-stdlib` is passed.

| Module | Contents |
| :--- | :--- |
| `core.lisp` | Aliases and boolean helpers |
| `math.lisp` | Constants, rounding, number theory, trigonometry, hyperbolics |
| `list.lisp` | List accessors and list operations |
| `iter.lisp` | `map`, `filter`, `reduce`, `each`, `any?`, `all?` |
| `types.lisp` | Type predicates |
| `io.lisp` | Printing, input, file modes, and file helpers |

See the [standard library reference](docs/stdlib.md).

## Documentation

| Document | Contents |
| :--- | :--- |
| [docs/language.md](docs/language.md) | Syntax, values, scoping, evaluation, error model |
| [docs/builtins.md](docs/builtins.md) | Special forms and native builtins |
| [docs/stdlib.md](docs/stdlib.md) | Functions from `std/` |
| [docs/c-api.md](docs/c-api.md) | C modules, ownership, GC contract, extension guide |

## Project layout

```text
glisp/
├── .github/workflows/     CI pipeline (Linux, macOS, Windows, leak gate)
├── CMakeLists.txt         Root build configuration
├── CMakePresets.json      Release, Debug (ASan/UBSan), Test, Leaks presets
├── Makefile               Developer entry point
├── conanfile.txt          Dependencies (readline)
├── main.lisp              Example script
├── src/
│   ├── main.c             CLI entry point and REPL loop
│   ├── lexer.c/.h         Tokenizer
│   ├── parser.c/.h        S-expression parser
│   ├── ast.c/.h           AST representation
│   ├── diagnostics.c/.h   Diagnostics and source snippets
│   ├── runtime/
│   │   ├── value.c/.h     Runtime values and constructors
│   │   ├── env.c/.h       Lexical environments
│   │   ├── eval.c/.h      Evaluator
│   │   ├── callstack.c/.h Call frames and stack traces
│   │   ├── memory.c/.h    Mark-and-sweep GC and temporary roots
│   │   ├── builtins.c/.h  Native functions
│   │   ├── specforms.c/.h Special forms
│   │   └── pragma.c/.h    Runtime directives
│   └── utils/             Typed containers and terminal helpers
├── std/                   Lisp standard library
├── tests/                 C test suites, Lisp fixtures, and the leak-check script
└── docs/                  Language, builtin, stdlib, and C API references
```

## Testing

```bash
make BUILD_TYPE=Test
make test
```

The test build enables AddressSanitizer and UndefinedBehaviorSanitizer. The
suite covers the lexer, parser, typed containers, the runtime, the standard
library, file I/O, and garbage-collector stress.

Run a single suite directly:

```bash
./build/tests/runtime_tests
```

The macOS `leaks` tool can be used to check a run for leaked allocations:

```bash
make BUILD_TYPE=Leaks
leaks --atExit -- ./build/glisp tests/leak.lisp
```

## Continuous integration

`.github/workflows/ci.yml` builds and tests the project on:

- **Linux** — GCC, Debug with AddressSanitizer/UndefinedBehaviorSanitizer,
  plus a Release build.
- **macOS** — Apple Clang, Debug with AddressSanitizer/UndefinedBehaviorSanitizer.
- **Windows** — MSYS2 MinGW-w64, Debug and Release, built with `make USE_CONAN=0`.
- **macOS leak gate** — `leaks --atExit -- ./glisp tests/leak.lisp` must report
  zero leaks.

The leak gate runs the interpreter on a dedicated script that exercises
closures, collections, higher-order functions, file I/O, and garbage
collection.

## Contributing

- Format C sources with `clang-format` (configuration: `.clang-format`) and
  keep `clang-tidy` checks clean (`.clang-tidy`).
- Run the test suite before submitting changes.
- Follow the existing naming and ownership conventions.
- Add tests for behavior changes; runtime values must respect the GC rules in
  [docs/c-api.md](docs/c-api.md).

A pre-commit configuration is provided in `.pre-commit-config.yaml`.

## License

GLisp is distributed under the GNU General Public License v3.0. See
[LICENSE](LICENSE).

## Acknowledgements

The REPL uses [GNU Readline](https://tiswww.case.edu/php/chet/readline/rltop.html).
