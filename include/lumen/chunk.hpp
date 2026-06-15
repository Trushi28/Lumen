#pragma once
#include "value.hpp"
#include <cstdint>
#include <vector>

namespace lumen {

// ── Opcodes ───────────────────────────────────────────────────────────────────
enum class Op : uint8_t {
    // Literals / stack
    CONST,         // CONST  idx8         — push constants[idx]
    NIL,           //                       push nil
    TRUE_OP,       //                       push true
    FALSE_OP,      //                       push false
    POP,           //                       pop top

    // Globals
    DEF_GLOBAL,    // DEF_GLOBAL idx8     — globals[name] = pop()
    GET_GLOBAL,    // GET_GLOBAL idx8
    SET_GLOBAL,    // SET_GLOBAL idx8

    // Locals
    GET_LOCAL,     // GET_LOCAL  slot8
    SET_LOCAL,     // SET_LOCAL  slot8

    // Upvalues (closures)
    GET_UPVAL,     // GET_UPVAL  idx8
    SET_UPVAL,     // SET_UPVAL  idx8
    CLOSE_UPVAL,   //            close top open upvalue + pop

    // Arithmetic
    ADD, SUB, MUL, DIV, MOD,
    NEG,

    // Comparison
    EQ, NEQ, LT, LTE, GT, GTE,

    // Logical
    NOT,

    // Control flow  (2-byte big-endian offset relative to next instruction)
    JUMP,          // unconditional forward jump
    JUMP_FALSE,    // jump if top falsy (does NOT pop)
    LOOP,          // backwards jump (for while/for)

    // Functions
    CLOSURE,       // CLOSURE fn_idx8 [is_local8 idx8] × upvalue_count
    CALL,          // CALL    argc8
    RETURN,

    // I/O
    PRINT,

    HALT,
};

// ── Chunk ─────────────────────────────────────────────────────────────────────
struct Chunk {
    std::vector<uint8_t> code;
    std::vector<Value>   constants;
    std::vector<int>     lines;     // parallel to code, for error reporting

    // ── Emit ─────────────────────────────────────────────────────────────────
    void emit(uint8_t byte, int line) {
        code.push_back(byte);
        lines.push_back(line);
    }
    void emit(Op op, int line) { emit(static_cast<uint8_t>(op), line); }

    // Emit a 2-byte big-endian offset (filled with placeholder 0xFFFF)
    // Returns the index of the first byte so it can be patched later.
    size_t emitJump(Op op, int line) {
        emit(op, line);
        emit(0xFF, line);
        emit(0xFF, line);
        return code.size() - 2; // index of the first offset byte
    }

    // Back-patch a previously emitted jump to point to the current position.
    void patchJump(size_t offset_idx) {
        size_t jump = code.size() - offset_idx - 2;
        if (jump > 0xFFFF) throw std::runtime_error("Jump too large");
        code[offset_idx]     = static_cast<uint8_t>(jump >> 8);
        code[offset_idx + 1] = static_cast<uint8_t>(jump & 0xFF);
    }

    // Emit a LOOP instruction that jumps back to `loop_start`.
    void emitLoop(size_t loop_start, int line) {
        emit(Op::LOOP, line);
        size_t offset = code.size() - loop_start + 2;
        if (offset > 0xFFFF) throw std::runtime_error("Loop body too large");
        emit(static_cast<uint8_t>(offset >> 8),  line);
        emit(static_cast<uint8_t>(offset & 0xFF), line);
    }

    // ── Constants ─────────────────────────────────────────────────────────────
    uint8_t addConstant(Value v) {
        constants.push_back(std::move(v));
        return static_cast<uint8_t>(constants.size() - 1);
    }

    // ── Debug disassembly ─────────────────────────────────────────────────────
    void disassemble(const std::string& name) const;
    size_t disassembleInstr(size_t offset) const;
};

} // namespace lumen
