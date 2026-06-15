#include "lumen/compiler.hpp"
#include "lumen/chunk.hpp"
#include "lumen/value.hpp"
#include <charconv>
#include <format>
#include <stdexcept>

namespace lumen {

// ── ObjFunction constructor (needs Chunk to be complete) ─────────────────────
ObjFunction::ObjFunction() : chunk(std::make_shared<Chunk>()) {
    objType = Obj::Type::Function;
}

Value ObjFunction::make(std::string name_, int arity_) {
    auto fn = std::make_shared<ObjFunction>();
    fn->name  = std::move(name_);
    fn->arity = arity_;
    return Value::fromObj(fn);
}

// ── Value helpers ─────────────────────────────────────────────────────────────
bool Value::operator==(const Value& o) const noexcept {
    if (tag != o.tag) return false;
    switch (tag) {
        case Tag::Nil:    return true;
        case Tag::Bool:   return b == o.b;
        case Tag::Number: return n == o.n;
        case Tag::Obj:
            if (isString() && o.isString())
                return asString()->chars == o.asString()->chars;
            return obj.get() == o.obj.get();
    }
    return false;
}

std::string Value::toString() const {
    switch (tag) {
        case Tag::Nil:    return "nil";
        case Tag::Bool:   return b ? "true" : "false";
        case Tag::Number: {
            double d = n;
            if (d == static_cast<int64_t>(d))
                return std::to_string(static_cast<int64_t>(d));
            return std::to_string(d);
        }
        case Tag::Obj:
            if (isString())   return asString()->chars;
            if (isFunction()) return std::format("<fn {}>",   asFunction()->name);
            if (isClosure())  return std::format("<fn {}>",   asClosure()->fn->name);
            if (isNative())   return std::format("<native {}>", asNative()->name);
            return "<obj>";
    }
    return "nil";
}

// ── Chunk disassembly ─────────────────────────────────────────────────────────
void Chunk::disassemble(const std::string& name) const {
    std::printf("=== %s ===\n", name.c_str());
    for (size_t i = 0; i < code.size(); )
        i = disassembleInstr(i);
}

size_t Chunk::disassembleInstr(size_t off) const {
    std::printf("%04zu  ", off);
    if (off > 0 && lines[off] == lines[off - 1]) std::printf("   | ");
    else std::printf("%4d ", lines[off]);

    auto op = static_cast<Op>(code[off]);
    auto byte1 = [&]{ return code[off + 1]; };
    auto offset16 = [&]{ return (code[off+1] << 8) | code[off+2]; };

    switch (op) {
        case Op::CONST:       std::printf("CONST        %3d  '%s'\n", byte1(), constants[byte1()].toString().c_str()); return off + 2;
        case Op::NIL:         std::printf("NIL\n");          return off + 1;
        case Op::TRUE_OP:     std::printf("TRUE\n");         return off + 1;
        case Op::FALSE_OP:    std::printf("FALSE\n");        return off + 1;
        case Op::POP:         std::printf("POP\n");          return off + 1;
        case Op::DEF_GLOBAL:  std::printf("DEF_GLOBAL   %3d  '%s'\n", byte1(), constants[byte1()].toString().c_str()); return off + 2;
        case Op::GET_GLOBAL:  std::printf("GET_GLOBAL   %3d  '%s'\n", byte1(), constants[byte1()].toString().c_str()); return off + 2;
        case Op::SET_GLOBAL:  std::printf("SET_GLOBAL   %3d  '%s'\n", byte1(), constants[byte1()].toString().c_str()); return off + 2;
        case Op::GET_LOCAL:   std::printf("GET_LOCAL    %3d\n", byte1()); return off + 2;
        case Op::SET_LOCAL:   std::printf("SET_LOCAL    %3d\n", byte1()); return off + 2;
        case Op::GET_UPVAL:   std::printf("GET_UPVAL    %3d\n", byte1()); return off + 2;
        case Op::SET_UPVAL:   std::printf("SET_UPVAL    %3d\n", byte1()); return off + 2;
        case Op::CLOSE_UPVAL: std::printf("CLOSE_UPVAL\n"); return off + 1;
        case Op::ADD:         std::printf("ADD\n");          return off + 1;
        case Op::SUB:         std::printf("SUB\n");          return off + 1;
        case Op::MUL:         std::printf("MUL\n");          return off + 1;
        case Op::DIV:         std::printf("DIV\n");          return off + 1;
        case Op::MOD:         std::printf("MOD\n");          return off + 1;
        case Op::NEG:         std::printf("NEG\n");          return off + 1;
        case Op::EQ:          std::printf("EQ\n");           return off + 1;
        case Op::NEQ:         std::printf("NEQ\n");          return off + 1;
        case Op::LT:          std::printf("LT\n");           return off + 1;
        case Op::LTE:         std::printf("LTE\n");          return off + 1;
        case Op::GT:          std::printf("GT\n");           return off + 1;
        case Op::GTE:         std::printf("GTE\n");          return off + 1;
        case Op::NOT:         std::printf("NOT\n");          return off + 1;
        case Op::JUMP:        std::printf("JUMP         %+4d\n", offset16()); return off + 3;
        case Op::JUMP_FALSE:  std::printf("JUMP_FALSE   %+4d\n", offset16()); return off + 3;
        case Op::LOOP:        std::printf("LOOP         %+4d\n", offset16()); return off + 3;
        case Op::CALL:        std::printf("CALL         %3d\n", byte1()); return off + 2;
        case Op::RETURN:      std::printf("RETURN\n");       return off + 1;
        case Op::PRINT:       std::printf("PRINT\n");        return off + 1;
        case Op::HALT:        std::printf("HALT\n");         return off + 1;
        case Op::CLOSURE: {
            uint8_t fn_idx = byte1();
            std::printf("CLOSURE      %3d  '%s'\n", fn_idx, constants[fn_idx].toString().c_str());
            auto fn = constants[fn_idx].asFunction();
            size_t o = off + 2;
            for (int i = 0; i < fn->upvalue_count; ++i) {
                std::printf("   %04zu   |   %s %d\n", o, code[o] ? "local" : "upval", code[o+1]);
                o += 2;
            }
            return o;
        }
        default: std::printf("UNKNOWN %d\n", (int)op); return off + 1;
    }
}

// ── Compiler ──────────────────────────────────────────────────────────────────

void Compiler::error(const std::string& msg, int line) {
    std::fprintf(stderr, "[line %d] Compile error: %s\n", line, msg.c_str());
    _had_error = true;
}

Chunk& Compiler::chunk() { return *_ctx->fn->chunk; }

void Compiler::emit(Op op, int line)             { chunk().emit(op, line); }
void Compiler::emit2(Op op, uint8_t a, int line) { chunk().emit(op, line); chunk().emit(a, line); }

uint8_t Compiler::makeConst(Value v, int line) {
    uint8_t idx = chunk().addConstant(std::move(v));
    return idx;
}

void Compiler::emitConst(Value v, int line) {
    emit2(Op::CONST, makeConst(std::move(v), line), line);
}

void Compiler::pushContext(FnContext& ctx, const std::string& name, int arity) {
    ctx.enclosing = _ctx;
    ctx.fn = std::make_shared<ObjFunction>();
    ctx.fn->name  = name;
    ctx.fn->arity = arity;
    _ctx = &ctx;

    // Reserve slot 0 for "this function" (so locals start at 1)
    ctx.locals.push_back({"", 0, false});
}

std::shared_ptr<ObjFunction> Compiler::popContext() {
    emit(Op::NIL,    0);
    emit(Op::RETURN, 0);
    auto fn = _ctx->fn;
    fn->upvalue_count = static_cast<int>(_ctx->upvalues.size());
    _ctx = _ctx->enclosing;
    return fn;
}

void Compiler::beginScope() { ++_ctx->scope_depth; }

void Compiler::endScope(int line) {
    --_ctx->scope_depth;
    // Pop locals that belong to the just-closed scope
    while (!_ctx->locals.empty() &&
           _ctx->locals.back().depth > _ctx->scope_depth) {
        if (_ctx->locals.back().captured)
            emit(Op::CLOSE_UPVAL, line);
        else
            emit(Op::POP, line);
        _ctx->locals.pop_back();
    }
}

// ── Variable resolution ───────────────────────────────────────────────────────

int Compiler::resolveLocal(FnContext* ctx, const std::string& name) {
    for (int i = static_cast<int>(ctx->locals.size()) - 1; i >= 0; --i) {
        if (ctx->locals[i].name == name) {
            if (ctx->locals[i].depth == -1)
                error("Can't read local variable in its own initialiser", 0);
            return i;
        }
    }
    return -1;
}

int Compiler::addUpvalue(FnContext* ctx, uint8_t idx, bool is_local) {
    // Deduplicate
    for (int i = 0; i < (int)ctx->upvalues.size(); ++i)
        if (ctx->upvalues[i].index == idx && ctx->upvalues[i].is_local == is_local)
            return i;
    ctx->upvalues.push_back({idx, is_local});
    return static_cast<int>(ctx->upvalues.size()) - 1;
}

int Compiler::resolveUpvalue(FnContext* ctx, const std::string& name) {
    if (!ctx->enclosing) return -1;

    int local = resolveLocal(ctx->enclosing, name);
    if (local != -1) {
        ctx->enclosing->locals[local].captured = true;
        return addUpvalue(ctx, static_cast<uint8_t>(local), true);
    }

    int upval = resolveUpvalue(ctx->enclosing, name);
    if (upval != -1)
        return addUpvalue(ctx, static_cast<uint8_t>(upval), false);

    return -1;
}

// ── Top-level entry ───────────────────────────────────────────────────────────

std::shared_ptr<ObjFunction> Compiler::compile(const std::vector<StmtPtr>& stmts) {
    FnContext top;
    pushContext(top, "<script>", 0);

    for (const auto& s : stmts)
        if (s) compileStmt(*s);

    auto fn = popContext();
    if (_had_error) throw std::runtime_error("Compilation failed");
    return fn;
}

// ── Statement dispatch ────────────────────────────────────────────────────────

void Compiler::compileStmt(const Stmt& s) {
    std::visit([this](const auto& n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, ExprStmt>)   compileExprS(n);
        if constexpr (std::is_same_v<T, PrintStmt>)  compilePrint(n);
        if constexpr (std::is_same_v<T, VarDecl>)    compileVarDecl(n);
        if constexpr (std::is_same_v<T, BlockStmt>)  compileBlock(n);
        if constexpr (std::is_same_v<T, IfStmt>)     compileIf(n);
        if constexpr (std::is_same_v<T, WhileStmt>)  compileWhile(n);
        if constexpr (std::is_same_v<T, FnDecl>)     compileFn(n);
        if constexpr (std::is_same_v<T, ReturnStmt>) compileReturn(n);
    }, s.v);
}

void Compiler::compileExprS(const ExprStmt& s) {
    compileExpr(*s.expr);
    emit(Op::POP, s.expr->line());
}

void Compiler::compilePrint(const PrintStmt& s) {
    compileExpr(*s.expr);
    emit(Op::PRINT, s.expr->line());
}

void Compiler::compileVarDecl(const VarDecl& s) {
    int line = s.name.line;

    // Compile initialiser (or push nil)
    if (s.init) compileExpr(*s.init);
    else        emit(Op::NIL, line);

    if (_ctx->scope_depth == 0) {
        // Global variable
        uint8_t idx = makeConst(ObjString::make(s.name.lexeme), line);
        emit2(Op::DEF_GLOBAL, idx, line);
    } else {
        // Local variable — mark as being initialised
        _ctx->locals.push_back({s.name.lexeme, -1, false});
        _ctx->locals.back().depth = _ctx->scope_depth;
    }
}

void Compiler::compileBlock(const BlockStmt& s) {
    int line = s.body.empty() ? 0 : s.body.front()->line();
    beginScope();
    for (const auto& stmt : s.body) if (stmt) compileStmt(*stmt);
    endScope(line);
}

void Compiler::compileIf(const IfStmt& s) {
    int line = s.cond->line();
    compileExpr(*s.cond);
    size_t then_jump = chunk().emitJump(Op::JUMP_FALSE, line);
    emit(Op::POP, line);              // pop condition (true branch)

    compileStmt(*s.then_br);

    size_t else_jump = chunk().emitJump(Op::JUMP, line);
    chunk().patchJump(then_jump);
    emit(Op::POP, line);              // pop condition (false branch)

    if (s.else_br) compileStmt(*s.else_br);
    chunk().patchJump(else_jump);
}

void Compiler::compileWhile(const WhileStmt& s) {
    int line = s.cond->line();
    size_t loop_start = chunk().code.size();

    compileExpr(*s.cond);
    size_t exit_jump = chunk().emitJump(Op::JUMP_FALSE, line);
    emit(Op::POP, line);

    compileStmt(*s.body);
    chunk().emitLoop(loop_start, line);

    chunk().patchJump(exit_jump);
    emit(Op::POP, line);
}

void Compiler::compileFn(const FnDecl& s) {
    int line = s.name.line;

    // Compile the function body into a new context
    FnContext fn_ctx;
    pushContext(fn_ctx, s.name.lexeme, static_cast<int>(s.params.size()));

    beginScope();
    for (const auto& p : s.params)
        _ctx->locals.push_back({p.lexeme, _ctx->scope_depth, false});

    for (const auto& stmt : s.body) if (stmt) compileStmt(*stmt);
    endScope(line);

    auto fn = popContext();
    fn->upvalue_count = static_cast<int>(fn_ctx.upvalues.size());

    // Emit CLOSURE instruction
    uint8_t fn_idx = makeConst(Value::fromObj(fn), line);
    chunk().emit(Op::CLOSURE, line);
    chunk().emit(fn_idx, line);
    for (const auto& uv : fn_ctx.upvalues) {
        chunk().emit(uv.is_local ? 1 : 0, line);
        chunk().emit(uv.index, line);
    }

    // Bind the closure to the name
    if (_ctx->scope_depth == 0) {
        uint8_t idx = makeConst(ObjString::make(s.name.lexeme), line);
        emit2(Op::DEF_GLOBAL, idx, line);
    } else {
        _ctx->locals.push_back({s.name.lexeme, _ctx->scope_depth, false});
    }
}

void Compiler::compileReturn(const ReturnStmt& s) {
    int line = s.keyword.line;
    if (s.value) compileExpr(*s.value);
    else         emit(Op::NIL, line);
    emit(Op::RETURN, line);
}

// ── Expression dispatch ───────────────────────────────────────────────────────

void Compiler::compileExpr(const Expr& e) {
    std::visit([this](const auto& n) {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LiteralExpr>)  compileLiteral(n);
        if constexpr (std::is_same_v<T, VarExpr>)      compileVar(n);
        if constexpr (std::is_same_v<T, AssignExpr>)   compileAssign(n);
        if constexpr (std::is_same_v<T, UnaryExpr>)    compileUnary(n);
        if constexpr (std::is_same_v<T, BinaryExpr>)   compileBinary(n);
        if constexpr (std::is_same_v<T, LogicalExpr>)  compileLogical(n);
        if constexpr (std::is_same_v<T, CallExpr>)     compileCall(n);
    }, e.v);
}

void Compiler::compileLiteral(const LiteralExpr& e) {
    int line = e.tok.line;
    switch (e.tok.type) {
        case TT::NIL_KW:   emit(Op::NIL,      line); break;
        case TT::TRUE_KW:  emit(Op::TRUE_OP,  line); break;
        case TT::FALSE_KW: emit(Op::FALSE_OP, line); break;
        case TT::NUMBER: {
            double d = std::stod(e.tok.lexeme);
            emitConst(Value::number(d), line);
            break;
        }
        case TT::STRING:
            emitConst(ObjString::make(e.tok.lexeme), line);
            break;
        default: error("Unknown literal type", line);
    }
}

void Compiler::compileVar(const VarExpr& e) {
    int line = e.name.line;
    int slot = resolveLocal(_ctx, e.name.lexeme);
    if (slot != -1) {
        emit2(Op::GET_LOCAL, static_cast<uint8_t>(slot), line);
        return;
    }
    int uv = resolveUpvalue(_ctx, e.name.lexeme);
    if (uv != -1) {
        emit2(Op::GET_UPVAL, static_cast<uint8_t>(uv), line);
        return;
    }
    uint8_t idx = makeConst(ObjString::make(e.name.lexeme), line);
    emit2(Op::GET_GLOBAL, idx, line);
}

void Compiler::compileAssign(const AssignExpr& e) {
    int line = e.name.line;
    compileExpr(*e.value);

    int slot = resolveLocal(_ctx, e.name.lexeme);
    if (slot != -1) { emit2(Op::SET_LOCAL, static_cast<uint8_t>(slot), line); return; }

    int uv = resolveUpvalue(_ctx, e.name.lexeme);
    if (uv != -1) { emit2(Op::SET_UPVAL, static_cast<uint8_t>(uv), line); return; }

    uint8_t idx = makeConst(ObjString::make(e.name.lexeme), line);
    emit2(Op::SET_GLOBAL, idx, line);
}

void Compiler::compileUnary(const UnaryExpr& e) {
    int line = e.op.line;
    compileExpr(*e.right);
    switch (e.op.type) {
        case TT::MINUS: emit(Op::NEG, line); break;
        case TT::BANG:
        case TT::NOT:   emit(Op::NOT, line); break;
        default: error("Unknown unary op", line);
    }
}

void Compiler::compileBinary(const BinaryExpr& e) {
    int line = e.op.line;
    compileExpr(*e.left);
    compileExpr(*e.right);
    switch (e.op.type) {
        case TT::PLUS:    emit(Op::ADD, line); break;
        case TT::MINUS:   emit(Op::SUB, line); break;
        case TT::STAR:    emit(Op::MUL, line); break;
        case TT::SLASH:   emit(Op::DIV, line); break;
        case TT::PERCENT: emit(Op::MOD, line); break;
        case TT::EQ_EQ:   emit(Op::EQ,  line); break;
        case TT::BANG_EQ: emit(Op::NEQ, line); break;
        case TT::LT:      emit(Op::LT,  line); break;
        case TT::LT_EQ:   emit(Op::LTE, line); break;
        case TT::GT:      emit(Op::GT,  line); break;
        case TT::GT_EQ:   emit(Op::GTE, line); break;
        default: error("Unknown binary op", line);
    }
}

void Compiler::compileLogical(const LogicalExpr& e) {
    int line = e.op.line;
    compileExpr(*e.left);

    if (e.op.type == TT::OR) {
        // short-circuit: if left is truthy, skip right
        size_t skip = chunk().emitJump(Op::JUMP_FALSE, line);
        size_t end  = chunk().emitJump(Op::JUMP, line);
        chunk().patchJump(skip);
        emit(Op::POP, line);
        compileExpr(*e.right);
        chunk().patchJump(end);
    } else { // AND
        size_t end = chunk().emitJump(Op::JUMP_FALSE, line);
        emit(Op::POP, line);
        compileExpr(*e.right);
        chunk().patchJump(end);
    }
}

void Compiler::compileCall(const CallExpr& e) {
    int line = e.paren.line;
    compileExpr(*e.callee);
    for (const auto& arg : e.args) compileExpr(*arg);
    emit2(Op::CALL, static_cast<uint8_t>(e.args.size()), line);
}

} // namespace lumen
