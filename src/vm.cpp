#include "lumen/vm.hpp"
#include "lumen/chunk.hpp"
#include "lumen/value.hpp"
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <format>
#include <stdexcept>

namespace lumen {

// ── VM constructor ────────────────────────────────────────────────────────────
VM::VM() { defineNatives(); }

// ── Native functions ──────────────────────────────────────────────────────────
void VM::defineNatives() {
    auto def = [this](const char* name, NativeFn fn) {
        _globals[name] = ObjNative::make(name, std::move(fn));
    };

    def("clock", [](int, Value*) -> Value {
        return Value::number(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
    });

    def("str", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value::nil();
        return ObjString::make(argv[0].toString());
    });

    def("num", [](int argc, Value* argv) -> Value {
        if (argc != 1 || !argv[0].isString()) return Value::nil();
        try { return Value::number(std::stod(argv[0].asString()->chars)); }
        catch (...) { return Value::nil(); }
    });

    def("len", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value::nil();
        if (argv[0].isString())
            return Value::number(static_cast<double>(argv[0].asString()->chars.size()));
        return Value::nil();
    });

    def("type", [](int argc, Value* argv) -> Value {
        if (argc != 1) return Value::nil();
        const Value& v = argv[0];
        if (v.isNil())      return ObjString::make("nil");
        if (v.isBool())     return ObjString::make("bool");
        if (v.isNumber())   return ObjString::make("number");
        if (v.isString())   return ObjString::make("string");
        if (v.isClosure() || v.isNative()) return ObjString::make("function");
        return ObjString::make("unknown");
    });
}

// ── run() ─────────────────────────────────────────────────────────────────────
InterpResult VM::run(std::shared_ptr<ObjFunction> script) {
    // Wrap the script function in a closure and push it as the implicit slot 0
    auto cval = ObjClosure::make(script);
    push(cval);

    CallFrame& f = _frames[_frame_count++];
    f.closure = cval.asClosure();
    f.ip      = script->chunk->code.data();
    f.base    = 0;

    return execute();
}

// ── Upvalue helpers ───────────────────────────────────────────────────────────
std::shared_ptr<ObjUpvalue> VM::captureUpvalue(Value* local) {
    // Walk the sorted open-upvalue list (highest stack address first)
    std::shared_ptr<ObjUpvalue>* prev = &_open_upvalues;
    auto                          curr = _open_upvalues;

    while (curr && curr->location > local) {
        prev = &curr->next;
        curr = curr->next;
    }
    if (curr && curr->location == local) return curr;   // already captured

    auto uv = std::make_shared<ObjUpvalue>(local);
    uv->next = curr;
    *prev    = uv;
    return uv;
}

void VM::closeUpvalues(Value* last) {
    // Close every open upvalue that points at or above `last` in the stack
    while (_open_upvalues && _open_upvalues->location >= last) {
        _open_upvalues->close();
        _open_upvalues = _open_upvalues->next;
    }
}

// ── Call dispatch ─────────────────────────────────────────────────────────────
bool VM::callClosure(ObjClosure* closure, int argc) {
    if (argc != closure->fn->arity) {
        runtimeError("Expected %d arguments but got %d",
                     closure->fn->arity, argc);
        return false;
    }
    if (_frame_count == FRAMES_MAX) {
        runtimeError("Stack overflow");
        return false;
    }
    CallFrame& f = _frames[_frame_count++];
    f.closure = closure;
    f.ip      = closure->fn->chunk->code.data();
    // Stack layout: [fn, arg0, arg1, ...] — base points to fn
    f.base    = static_cast<int>(_sp - _stack) - argc - 1;
    return true;
}

bool VM::callValue(Value callee, int argc) {
    if (callee.isClosure()) {
        return callClosure(callee.asClosure(), argc);
    }
    if (callee.isNative()) {
        ObjNative* native = callee.asNative();
        Value result = native->fn(argc, _sp - argc);
        _sp -= argc + 1;    // remove args + function slot
        push(result);
        return true;
    }
    runtimeError("Can only call functions and closures");
    return false;
}

// ── Error reporting ───────────────────────────────────────────────────────────
InterpResult VM::runtimeError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);

    // Print call stack trace (innermost first)
    for (int i = _frame_count - 1; i >= 0; --i) {
        const CallFrame& fr  = _frames[i];
        const ObjFunction* fn = fr.closure->fn.get();
        size_t instr = static_cast<size_t>(fr.ip - fn->chunk->code.data()) - 1;
        int line     = fn->chunk->lines[instr];
        std::fprintf(stderr, "  [line %d] in %s\n", line,
                     fn->name.empty() ? "<script>" : fn->name.c_str());
    }

    _sp          = _stack;
    _frame_count = 0;
    return InterpResult::RUNTIME_ERROR;
}

// ════════════════════════════════════════════════════════════════════════════
// Main execution loop
// ════════════════════════════════════════════════════════════════════════════
InterpResult VM::execute() {

// Fast read macros (operate on current frame)
#define FRAME       (_frames[_frame_count - 1])
#define READ_BYTE() (*FRAME.ip++)
#define READ_SHORT() (FRAME.ip += 2, static_cast<uint16_t>((FRAME.ip[-2] << 8) | FRAME.ip[-1]))
#define READ_CONST() (FRAME.closure->fn->chunk->constants[READ_BYTE()])
#define RUNTIME_ERR(...) return runtimeError(__VA_ARGS__)

    while (true) {
        Op op = static_cast<Op>(READ_BYTE());

        switch (op) {

        // ── Literals ─────────────────────────────────────────────────────────
        case Op::CONST:    push(READ_CONST());         break;
        case Op::NIL:      push(Value::nil());         break;
        case Op::TRUE_OP:  push(Value::boolean(true)); break;
        case Op::FALSE_OP: push(Value::boolean(false));break;
        case Op::POP:      pop();                      break;

        // ── Globals ───────────────────────────────────────────────────────────
        case Op::DEF_GLOBAL: {
            std::string name = READ_CONST().asString()->chars;
            _globals[std::move(name)] = pop();
            break;
        }
        case Op::GET_GLOBAL: {
            const std::string& name = READ_CONST().asString()->chars;
            auto it = _globals.find(name);
            if (it == _globals.end())
                RUNTIME_ERR("Undefined variable '%s'", name.c_str());
            push(it->second);
            break;
        }
        case Op::SET_GLOBAL: {
            const std::string& name = READ_CONST().asString()->chars;
            auto it = _globals.find(name);
            if (it == _globals.end())
                RUNTIME_ERR("Undefined variable '%s'", name.c_str());
            it->second = peek(0);   // assignment is an expression — don't pop
            break;
        }

        // ── Locals ───────────────────────────────────────────────────────────
        case Op::GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(_stack[FRAME.base + slot]);
            break;
        }
        case Op::SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            _stack[FRAME.base + slot] = peek(0);
            break;
        }

        // ── Upvalues ─────────────────────────────────────────────────────────
        case Op::GET_UPVAL: {
            uint8_t idx = READ_BYTE();
            push(*FRAME.closure->upvalues[idx]->location);
            break;
        }
        case Op::SET_UPVAL: {
            uint8_t idx = READ_BYTE();
            *FRAME.closure->upvalues[idx]->location = peek(0);
            break;
        }
        case Op::CLOSE_UPVAL:
            closeUpvalues(_sp - 1);
            pop();
            break;

        // ── Arithmetic ────────────────────────────────────────────────────────
        case Op::ADD: {
            if (peek(0).isString() && peek(1).isString()) {
                // String concatenation
                std::string b = pop().asString()->chars;
                std::string a = pop().asString()->chars;
                push(ObjString::make(a + b));
            } else if (peek(0).isNumber() && peek(1).isNumber()) {
                double b = pop().asNumber(), a = pop().asNumber();
                push(Value::number(a + b));
            } else {
                RUNTIME_ERR("Operands to '+' must be two numbers or two strings");
            }
            break;
        }
        case Op::SUB: {
            if (!peek(0).isNumber() || !peek(1).isNumber())
                RUNTIME_ERR("Operands to '-' must be numbers");
            double b = pop().asNumber(), a = pop().asNumber();
            push(Value::number(a - b));
            break;
        }
        case Op::MUL: {
            if (!peek(0).isNumber() || !peek(1).isNumber())
                RUNTIME_ERR("Operands to '*' must be numbers");
            double b = pop().asNumber(), a = pop().asNumber();
            push(Value::number(a * b));
            break;
        }
        case Op::DIV: {
            if (!peek(0).isNumber() || !peek(1).isNumber())
                RUNTIME_ERR("Operands to '/' must be numbers");
            double b = pop().asNumber();
            if (b == 0.0) RUNTIME_ERR("Division by zero");
            double a = pop().asNumber();
            push(Value::number(a / b));
            break;
        }
        case Op::MOD: {
            if (!peek(0).isNumber() || !peek(1).isNumber())
                RUNTIME_ERR("Operands to '%' must be numbers");
            double b = pop().asNumber();
            if (b == 0.0) RUNTIME_ERR("Modulo by zero");
            double a = pop().asNumber();
            push(Value::number(std::fmod(a, b)));
            break;
        }
        case Op::NEG: {
            if (!peek(0).isNumber()) RUNTIME_ERR("Operand to '-' must be a number");
            push(Value::number(-pop().asNumber()));
            break;
        }
        case Op::NOT:
            push(Value::boolean(!pop().isTruthy()));
            break;

        // ── Comparison ────────────────────────────────────────────────────────
        case Op::EQ:  { Value b = pop(), a = pop(); push(Value::boolean(a == b));    break; }
        case Op::NEQ: { Value b = pop(), a = pop(); push(Value::boolean(!(a == b))); break; }
        case Op::LT: {
            if (!peek(0).isNumber() || !peek(1).isNumber()) RUNTIME_ERR("Operands must be numbers");
            double b = pop().asNumber(), a = pop().asNumber(); push(Value::boolean(a < b)); break;
        }
        case Op::LTE: {
            if (!peek(0).isNumber() || !peek(1).isNumber()) RUNTIME_ERR("Operands must be numbers");
            double b = pop().asNumber(), a = pop().asNumber(); push(Value::boolean(a <= b)); break;
        }
        case Op::GT: {
            if (!peek(0).isNumber() || !peek(1).isNumber()) RUNTIME_ERR("Operands must be numbers");
            double b = pop().asNumber(), a = pop().asNumber(); push(Value::boolean(a > b)); break;
        }
        case Op::GTE: {
            if (!peek(0).isNumber() || !peek(1).isNumber()) RUNTIME_ERR("Operands must be numbers");
            double b = pop().asNumber(), a = pop().asNumber(); push(Value::boolean(a >= b)); break;
        }

        // ── Control flow ──────────────────────────────────────────────────────
        case Op::JUMP: {
            uint16_t offset = READ_SHORT();
            FRAME.ip += offset;
            break;
        }
        case Op::JUMP_FALSE: {
            uint16_t offset = READ_SHORT();
            if (!peek(0).isTruthy()) FRAME.ip += offset;
            break;
        }
        case Op::LOOP: {
            uint16_t offset = READ_SHORT();
            FRAME.ip -= offset;
            break;
        }

        // ── Closures ─────────────────────────────────────────────────────────
        case Op::CLOSURE: {
            uint8_t fn_idx = READ_BYTE();
            // The constant is stored as an ObjFunction
            auto fn_val = FRAME.closure->fn->chunk->constants[fn_idx];
            auto fn     = std::static_pointer_cast<ObjFunction>(fn_val.obj);
            auto cl     = std::make_shared<ObjClosure>(fn);

            for (int i = 0; i < fn->upvalue_count; ++i) {
                uint8_t is_local = READ_BYTE();
                uint8_t idx      = READ_BYTE();
                if (is_local)
                    cl->upvalues[i] = captureUpvalue(&_stack[FRAME.base + idx]);
                else
                    cl->upvalues[i] = FRAME.closure->upvalues[idx];
            }
            push(Value::fromObj(cl));
            break;
        }

        // ── Function call ─────────────────────────────────────────────────────
        case Op::CALL: {
            int argc = READ_BYTE();
            if (!callValue(peek(argc), argc))
                return InterpResult::RUNTIME_ERROR;
            // If it's a closure call, a new CallFrame was pushed; execution
            // continues in the new frame on the next loop iteration.
            break;
        }

        case Op::RETURN: {
            Value result       = pop();
            int   callee_base  = FRAME.base;

            // Close all upvalues belonging to this frame before unwinding
            closeUpvalues(_stack + callee_base);
            _frame_count--;

            if (_frame_count == 0) {
                // Script finished — done
                return InterpResult::OK;
            }

            // Restore stack pointer to the function's slot (removes fn + locals)
            _sp = _stack + callee_base;
            push(result);
            break;
        }

        // ── I/O ───────────────────────────────────────────────────────────────
        case Op::PRINT:
            std::printf("%s\n", pop().toString().c_str());
            break;

        case Op::HALT:
            return InterpResult::OK;

        default:
            RUNTIME_ERR("Unknown opcode %d", static_cast<int>(op));
        }
    }

#undef FRAME
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONST
#undef RUNTIME_ERR
}

} // namespace lumen
