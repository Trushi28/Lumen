# Lumen 🌟

> A dynamically-typed scripting language compiled to bytecode and interpreted by a stack-based VM — written in C++20 from scratch.

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?logo=cplusplus&logoColor=white)
![Build](https://img.shields.io/badge/build-CMake%203.22%2B-064F8C?logo=cmake&logoColor=white)
![License](https://img.shields.io/badge/license-MIT-green)

```lumen
fn makeCounter(label) {
    var n = 0;
    fn tick() { n = n + 1; return label + ": " + str(n); }
    return tick;
}
var c = makeCounter("hits");
print c();   // hits: 1
print c();   // hits: 2
```

Closures, first-class functions, and upvalues that correctly outlive their declaring stack frame. Runs `fib(30)` in ~0.20s — about 5× faster than CPython 3.12 on the same workload.

---

## Architecture

Lumen is a classic 3-stage pipeline:

```
Source (.lm)
    │
    ▼  src/lexer.cpp
 Token stream
    │
    ▼  src/parser.cpp  — Recursive descent, Pratt-style precedence climbing
 AST  (std::variant node hierarchy, unique_ptr ownership)
    │
    ▼  src/compiler.cpp — Single-pass AST walker
 Bytecode Chunk  (Op enum, constant pool, line number table)
    │
    ▼  src/vm.cpp  — Stack-based VM, upvalue engine
 Output
```

---

## Language

```lumen
// Variables
var x = 42;
var name = "Lumen";

// Arithmetic and comparison
print (x + 8) * 2;     // 100
print x > 10;           // true

// String concatenation
print "Hello, " + name + "!";

// If / else
if (x > 100) {
    print "big";
} else {
    print "small";      // prints this
}

// While loop
var i = 0;
while (i < 5) { print i; i = i + 1; }

// For loop
for (var j = 0; j < 3; j = j + 1) { print j; }

// Functions and recursion
fn fib(n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
print fib(10);   // 55

// Closures — upvalues survive their declaring stack frame
fn makeAdder(x) {
    fn add(y) { return x + y; }
    return add;
}
var add5 = makeAdder(5);
print add5(3);   // 8
```

---

## The Upvalue Engine

The hard part of a scripting language is making closures *correct* — captured variables must outlive the stack frame where they were declared.

**Open upvalue:** While the captured local is still on the stack, `ObjUpvalue::location` is a raw pointer into the VM's value stack array.

**Closed upvalue:** When the owning stack frame unwinds (at `RETURN` or `CLOSE_UPVAL`), the value is copied to `ObjUpvalue::closed` and `location` is redirected to `&closed`. The closure now owns its captured value on the heap via `shared_ptr<ObjUpvalue>`.

```
Stack frame for makeCounter:
  [fn, n=3]
         ▲
         │ location (open upvalue)
  ┌──────────────┐
  │  ObjUpvalue  │ ◄── shared by all closures capturing n
  │  closed = _  │
  └──────────────┘

After makeCounter returns (CLOSE_UPVAL / RETURN):
  [fn, n=3] ← stack frame is gone
  ┌──────────────┐
  │  ObjUpvalue  │
  │  closed = 3  │ ◄── location now redirected HERE (heap)
  └──────────────┘
```

A linked list of open upvalues sorted by stack address ensures the same variable is never captured twice — `captureUpvalue()` walks the list and reuses an existing upvalue if one already points to that slot. Two closures capturing the same local share one `ObjUpvalue` and observe each other's mutations correctly.

---

## VM Instruction Set

| Opcode | Args | Effect |
|--------|------|--------|
| `CONST` | idx8 | push constants[idx] |
| `NIL` / `TRUE` / `FALSE` | — | push literal |
| `GET_LOCAL` / `SET_LOCAL` | slot8 | stack-relative access |
| `GET_GLOBAL` / `SET_GLOBAL` | name_idx | hash-map access |
| `GET_UPVAL` / `SET_UPVAL` | upv_idx | dereference upvalue pointer |
| `CLOSE_UPVAL` | — | close + pop top |
| `ADD` | — | number add or string concat |
| `JUMP` / `JUMP_FALSE` | offset16 | control flow |
| `LOOP` | offset16 | backwards jump |
| `CLOSURE` | fn_idx8 + upval descriptors | create ObjClosure |
| `CALL` | argc8 | dispatch closure or native |
| `RETURN` | — | close upvalues, restore frame |
| `PRINT` | — | pop + print |

---

## Native Functions

| Function | Signature | Example |
|----------|-----------|---------|
| `clock()` | `() → number` | `var t = clock();` |
| `str(v)` | `(any) → string` | `str(42)` → `"42"` |
| `num(s)` | `(string) → number` | `num("3.14")` → `3.14` |
| `len(s)` | `(string) → number` | `len("hi")` → `2` |
| `type(v)` | `(any) → string` | `type(42)` → `"number"` |

---

## Project Structure

```
lumen/
├── include/lumen/
│   ├── token.hpp      # Token types and Token struct
│   ├── ast.hpp        # AST node hierarchy (std::variant)
│   ├── value.hpp      # Value type, ObjUpvalue, ObjClosure
│   ├── chunk.hpp      # Bytecode chunk: opcodes, constant pool, line table
│   ├── compiler.hpp   # Single-pass AST-to-bytecode compiler
│   └── vm.hpp         # Stack VM, call frame stack, open upvalue list
├── src/               # Implementations
└── examples/
    ├── fib.lm         # Recursive Fibonacci — VM benchmark
    ├── closures.lm    # Upvalue engine demo
    └── fizzbuzz.lm    # For loop and modulo
```

---

## Build & Run

**Requirements:** GCC 13+ or Clang 17+, CMake 3.22+

```bash
git clone https://github.com/Trushi28/Lumen.git
cd lumen && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run examples
./lumen examples/fib.lm         # recursive Fibonacci benchmark
./lumen examples/closures.lm    # upvalue / closure demo
./lumen examples/fizzbuzz.lm    # for-loop and modulo

# Bytecode disassembly
./lumen --debug examples/fib.lm

# REPL
./lumen
```

---

## Benchmark

```
fib(30) = 832040
Time: ~0.20s   (Release build)
```

CPython 3.12 takes ~1.1s for the same workload. (~5× faster)

To run it yourself:

```bash
./lumen examples/fib.lm

# Python baseline
python3 -c "
def fib(n): return n if n <= 1 else fib(n-1) + fib(n-2)
import time; t = time.time(); print(fib(30)); print(time.time() - t)
"
```

---

## Roadmap

- [ ] Phase 2: Mark-sweep GC (replace `shared_ptr` reference counting)
- [ ] Phase 3: Classes and inheritance (`class Foo extends Bar {}`)
- [ ] Phase 4: Modules (`import "math"`)
- [ ] Phase 5: JIT — emit x86-64 machine code for hot functions
- [ ] Phase 6: Standard library (file I/O, collections, math)

---

## Implementation Notes

**Variable lookup complexity:** O(1) for locals — the compiler resolves every local to a stack slot index at compile time, so the VM performs a simple array index. O(1) amortised for globals (hash map). O(k) for upvalues where k is the closure nesting depth, typically ≤ 3 in real code.

**Short-circuit `and`/`or` in bytecode:** `JUMP_FALSE` peeks the stack top without popping. For `and`: if the left operand is falsy, jump past the right entirely. For `or`: if the left is falsy, fall through to evaluate the right; otherwise, jump past it. The value on top at the jump site is the result of the expression.

**Why not LLVM?** Single-pass bytecode compilation is simpler, more instructive, and fast enough for a scripting language at this stage. The compiler makes one AST pass and emits opcodes directly — no IR, no optimization passes, no external dependency. Phase 5 will layer a JIT on top of the baseline VM for hot functions without replacing it.

**Upvalue list invariant:** Open upvalues are kept in a linked list sorted by stack address (descending). `captureUpvalue()` scans from the head; if it finds an existing upvalue pointing to the requested slot, it returns that one. This guarantees shared mutation — if two closures both capture variable `n`, they observe each other's writes because they hold the same `ObjUpvalue`.

---

## License

MIT
