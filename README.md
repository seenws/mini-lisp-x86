# Mini Lisp Compiler (x86_64)

A Common-Lisp-to-x86_64 compiler written in C from scratch — no external
dependencies, no compiler frameworks.

The full pipeline (lexer, parser, semantic analysis, three-address-code IR,
x86_64 codegen) works end-to-end for a growing language subset: source files
compile to native Linux executables.

---

## Quick start

```bash
make
cd src
```

```lisp
; average.lisp
(let ((a 10)
      (b 20))
  (/ (+ a b) 2))
```

```bash
$ ./lispc average.lisp                             # writes average.s
$ gcc average.s build/runtime/runtime.o -o average
$ ./average
15
```

A program returns the value of its last top-level form; the runtime prints
it. For the bundled test programs there's a make shortcut that does all
three steps:

```bash
$ make tests/stage_2/01_variable_binding.bin
$ ./tests/stage_2/01_variable_binding.bin
30
```

Requires GCC (or any C99 compiler) on x86_64 Linux. `make clean` removes
all build output, including generated `.s`/`.bin` files.

---

## Language support

Working today:

- Integers, string literals, `nil`, `t`, `()`
- Arithmetic `+ - * /` with Common Lisp semantics (variadic left-fold,
  unary negation/reciprocal)
- `let` bindings, including shadowing and nested scopes
- `if`, one- and two-armed, arbitrarily nested (only `nil` is false)

Not yet: `format`, comparison predicates, `defun`/`lambda`, quote, lists.
Unsupported forms are rejected with source-located diagnostics rather than
miscompiled.

---

## Project structure

```
src/
├── compiler/
│   ├── lexer.c       # Tokenization
│   ├── parser.c      # Parsing & AST construction
│   ├── ast.c         # AST node creation and management
│   ├── semantic.c    # Scope/arity validation, diagnostics
│   ├── ir.c          # AST → three-address code
│   └── codegen.c     # IR → x86_64 assembly
├── runtime/
│   └── runtime.c     # Process entry; prints the program's result
├── util/             # error context, strdup/strndup
├── tests/            # stage_{1,2,3}/*.lisp test programs
└── main.c            # driver
```

Design decisions are documented where they live: `compiler/ir.h` covers the
IR and the tagged value encoding, `compiler/codegen.h` the register
allocation strategy.

---

## Roadmap

- [x] Lexer with dispatch table
- [x] Token stream abstraction
- [x] AST generation
- [x] Semantic analysis
- [x] Intermediate representation (three-address code)
- [x] x86_64 code generation (spill everything)
- [x] Basic runtime system (result printing)
- [ ] `format` and the builtin calling convention
- [ ] Comparison predicates `< > =`
- [ ] Constant folding (AST → AST pass)
- [ ] Functions: `defun`, `lambda`, recursion
- [ ] Linear-scan register allocation

---
