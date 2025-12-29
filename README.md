# Mini Lisp Compiler (x86_64)

A Common-Lisp-to-x86_64 compiler written in C from scratch.

Currently in early development — implements a complete lexer, parser, and AST generation for basic Lisp expressions.

---

## Current Features

### Lexical Analysis
- **Dispatch table-based tokenization** for efficient character-level parsing
- Full support for:
  - Parentheses: `(`, `)`
  - Symbols: `format`, `t`, `my-variable`
  - Strings: `"hello world!"`
  - Numbers: `42`, `3.14`, `-17`
  - Operators: `+`, `-`, `*`, `/`
  - Keywords: `:example`, `:key`
  - Comments: `; like this`

### Parsing & AST Generation
- Recursive descent parser
- Abstract Syntax Tree (AST) with proper node types:
  - `NODE_SYMBOL`
  - `NODE_STRING`
  - `NODE_NUMBER`
  - `NODE_LIST`
  - `NODE_NIL`
- Memory-safe string handling with custom `strdup`/`strndup` implementations

### Example
```lisp
(format t "Hello, world!")
```

Produces a complete AST with properly typed nodes.

### Proof check:
```bash
gdb ./lispc
break main.c:57 # as of the time of writing, line 57 is where we free the token stream.
```
```gdb
run tests/stage_1/01_simple_print.lisp
p program->as.list.children[0]->as.symbol
p program->as.list.children[1]->as.symbol
p program->as.list.children[2]->as.symbol

p program->as.list.children[1].type
p program->as.list.children[2].type
p program->as.list.children[3].type
```
```gdb
$1 = 0x55555555aa30 "format"
$2 = 0x55555555aaa0 "t"
$3 = 0x55555555ab10 "Hello, World!"

$4 = NODE_SYMBOL
$5 = NODE_SYMBOL
$6 = NODE_STRING
```

---

## Building

### Requirements
- GCC or any C99-compatible compiler
- Standard C library (no external dependencies)

### Compile and Run
```bash
make
./lispc <file.lisp>
```

---

## Project Structure
```
src/
├── compiler/
│   ├── lexer.c       # Tokenization
│   ├── parser.c      # Parsing & AST construction
│   └── ast.c         # AST node creation and management
├── util/
│   ├── mlispc_strdup.c
│   └── mlispc_strndup.c
└── main.c
```

---

## Roadmap

- [x] Lexer with dispatch table
- [x] Token stream abstraction
- [x] AST generation
- [ ] Semantic analysis
- [ ] Intermediate representation (IR)
- [ ] x86_64 code generation
- [ ] Basic runtime system

---
