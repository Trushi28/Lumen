#pragma once
#include "ast.hpp"
#include "chunk.hpp"
#include "value.hpp"
#include <string>
#include <vector>

namespace lumen {

// ── Compile-time representation of a captured variable ───────────────────────
struct UpvalDesc {
    uint8_t index;     // slot in enclosing locals (is_local=true)
                       // or index in enclosing upvals (is_local=false)
    bool    is_local;
};

// ── Per-local variable tracking ───────────────────────────────────────────────
struct Local {
    std::string name;
    int         depth;    // scope depth when declared (-1 = being initialised)
    bool        captured; // true if a nested function captures this local
};

// ── Per-function compilation context ─────────────────────────────────────────
// Linked list: inner functions point back to their enclosing function's context.
struct FnContext {
    FnContext*                     enclosing{nullptr};
    std::shared_ptr<ObjFunction>   fn;
    std::vector<Local>             locals;
    std::vector<UpvalDesc>         upvalues;
    int                            scope_depth{0};
};

// ── Compiler ─────────────────────────────────────────────────────────────────
class Compiler {
  public:
    // Compile a top-level program and return the script function.
    // Throws std::runtime_error on compile errors.
    std::shared_ptr<ObjFunction> compile(const std::vector<StmtPtr>& stmts);

  private:
    FnContext*  _ctx{nullptr};   // current function context
    bool        _had_error{false};

    // ── Context management ────────────────────────────────────────────────────
    void        pushContext(FnContext& ctx, const std::string& name, int arity);
    std::shared_ptr<ObjFunction> popContext();
    Chunk&      chunk();

    // ── Scope management ──────────────────────────────────────────────────────
    void        beginScope();
    void        endScope(int line);

    // ── Variable resolution ───────────────────────────────────────────────────
    int  resolveLocal  (FnContext* ctx, const std::string& name);
    int  resolveUpvalue(FnContext* ctx, const std::string& name);
    int  addUpvalue    (FnContext* ctx, uint8_t idx, bool is_local);

    // ── Emit helpers ──────────────────────────────────────────────────────────
    void emit    (Op op, int line);
    void emit2   (Op op, uint8_t arg, int line);
    uint8_t makeConst(Value v, int line);
    void emitConst(Value v, int line);

    // ── Statement compilation ─────────────────────────────────────────────────
    void compileStmt   (const Stmt& s);
    void compileExprS  (const ExprStmt& s);
    void compilePrint  (const PrintStmt& s);
    void compileVarDecl(const VarDecl& s);
    void compileBlock  (const BlockStmt& s);
    void compileIf     (const IfStmt& s);
    void compileWhile  (const WhileStmt& s);
    void compileFn     (const FnDecl& s);
    void compileReturn (const ReturnStmt& s);

    // ── Expression compilation ────────────────────────────────────────────────
    void compileExpr   (const Expr& e);
    void compileLiteral(const LiteralExpr& e);
    void compileVar    (const VarExpr& e);
    void compileAssign (const AssignExpr& e);
    void compileUnary  (const UnaryExpr& e);
    void compileBinary (const BinaryExpr& e);
    void compileLogical(const LogicalExpr& e);
    void compileCall   (const CallExpr& e);

    // ── Error ─────────────────────────────────────────────────────────────────
    void error(const std::string& msg, int line);
};

} // namespace lumen
