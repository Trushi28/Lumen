#pragma once
#include "chunk.hpp"
#include "value.hpp"
#include <string>
#include <unordered_map>

namespace lumen {

// ── Call frame ────────────────────────────────────────────────────────────────
// One frame per active function call.
// ip points directly into the bytecode array in closure->fn->chunk.
struct CallFrame {
    ObjClosure* closure;   // the function executing in this frame
    uint8_t*    ip;        // instruction pointer
    int         base;      // index of first local (stack slot 0 for this frame)
};

// ── Result ────────────────────────────────────────────────────────────────────
enum class InterpResult { OK, COMPILE_ERROR, RUNTIME_ERROR };

// ── VM ────────────────────────────────────────────────────────────────────────
class VM {
  public:
    VM();

    InterpResult run(std::shared_ptr<ObjFunction> script);

  private:
    static constexpr int STACK_MAX  = 64 * 256;  // max values on the stack
    static constexpr int FRAMES_MAX = 256;        // max call depth

    Value      _stack[STACK_MAX];
    Value*     _sp{_stack};                       // stack pointer (points to next free slot)

    CallFrame  _frames[FRAMES_MAX];
    int        _frame_count{0};

    std::unordered_map<std::string, Value> _globals;
    std::shared_ptr<ObjUpvalue>            _open_upvalues; // sorted by stack pos (highest first)

    // ── Stack ops ─────────────────────────────────────────────────────────────
    void  push(Value v)  { *_sp++ = std::move(v); }
    Value pop()          { return std::move(*--_sp); }
    Value& peek(int n=0) { return *(_sp - 1 - n); } // peek(0) = top
    const Value& peek(int n=0) const { return *(_sp - 1 - n); }

    // ── Upvalue management ────────────────────────────────────────────────────
    std::shared_ptr<ObjUpvalue> captureUpvalue(Value* local);
    void closeUpvalues(Value* last); // close all upvalues >= last

    // ── Call dispatch ─────────────────────────────────────────────────────────
    bool callValue(Value callee, int argc);
    bool callClosure(ObjClosure* closure, int argc);
    CallFrame& frame() { return _frames[_frame_count - 1]; }

    // ── Main execution loop ───────────────────────────────────────────────────
    InterpResult execute();

    // ── Natives ───────────────────────────────────────────────────────────────
    void defineNatives();

    // ── Error ─────────────────────────────────────────────────────────────────
    InterpResult runtimeError(const char* fmt, ...);
};

} // namespace lumen
