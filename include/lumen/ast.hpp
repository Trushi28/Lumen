#pragma once
#include "token.hpp"
#include <memory>
#include <variant>
#include <vector>

namespace lumen {

// ── Forward declarations (needed for recursive structures) ────────────────────
struct Expr;
struct Stmt;
using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

// ══════════════════════════════════════════════════════════════════════════════
// Expression nodes
// ══════════════════════════════════════════════════════════════════════════════

struct LiteralExpr  { Token tok; };                     // nil, true, false, number, string
struct VarExpr      { Token name; };                    // variable read
struct AssignExpr   { Token name; ExprPtr value; };     // x = expr
struct UnaryExpr    { Token op;   ExprPtr right; };     // !x  -x
struct BinaryExpr   { ExprPtr left; Token op; ExprPtr right; };   // a+b  a<b  ...
struct LogicalExpr  { ExprPtr left; Token op; ExprPtr right; };   // and / or (short-circuit)
struct CallExpr     { ExprPtr callee; Token paren; std::vector<ExprPtr> args; };

// Expr is the sum type of all expression nodes.
// Using a wrapping struct so it can be forward-declared above.
struct Expr {
    std::variant<
        LiteralExpr, VarExpr, AssignExpr,
        UnaryExpr, BinaryExpr, LogicalExpr,
        CallExpr
    > v;

    // Source line — pulled from the variant
    int line() const;
};

// ── Expression factories ──────────────────────────────────────────────────────
inline ExprPtr makeLiteral(Token t) {
    return std::make_unique<Expr>(Expr{LiteralExpr{std::move(t)}});
}
inline ExprPtr makeVar(Token t) {
    return std::make_unique<Expr>(Expr{VarExpr{std::move(t)}});
}
inline ExprPtr makeAssign(Token name, ExprPtr val) {
    return std::make_unique<Expr>(Expr{AssignExpr{std::move(name), std::move(val)}});
}
inline ExprPtr makeUnary(Token op, ExprPtr right) {
    return std::make_unique<Expr>(Expr{UnaryExpr{std::move(op), std::move(right)}});
}
inline ExprPtr makeBinary(ExprPtr left, Token op, ExprPtr right) {
    return std::make_unique<Expr>(Expr{BinaryExpr{std::move(left), std::move(op), std::move(right)}});
}
inline ExprPtr makeLogical(ExprPtr left, Token op, ExprPtr right) {
    return std::make_unique<Expr>(Expr{LogicalExpr{std::move(left), std::move(op), std::move(right)}});
}
inline ExprPtr makeCall(ExprPtr callee, Token paren, std::vector<ExprPtr> args) {
    return std::make_unique<Expr>(Expr{CallExpr{std::move(callee), std::move(paren), std::move(args)}});
}

// ══════════════════════════════════════════════════════════════════════════════
// Statement nodes
// ══════════════════════════════════════════════════════════════════════════════

struct ExprStmt    { ExprPtr expr; };
struct PrintStmt   { ExprPtr expr; };
struct VarDecl     { Token name; ExprPtr init; /* init may be nullptr */ };
struct BlockStmt   { std::vector<StmtPtr> body; };
struct IfStmt      { ExprPtr cond; StmtPtr then_br; StmtPtr else_br; /* else_br may be null */ };
struct WhileStmt   { ExprPtr cond; StmtPtr body; };
struct FnDecl      { Token name; std::vector<Token> params; std::vector<StmtPtr> body; };
struct ReturnStmt  { Token keyword; ExprPtr value; /* value may be null */ };

struct Stmt {
    std::variant<
        ExprStmt, PrintStmt, VarDecl, BlockStmt,
        IfStmt, WhileStmt, FnDecl, ReturnStmt
    > v;

    int line() const;
};

// ── Statement factories ───────────────────────────────────────────────────────
inline StmtPtr makeExprStmt(ExprPtr e)     { return std::make_unique<Stmt>(Stmt{ExprStmt{std::move(e)}}); }
inline StmtPtr makePrintStmt(ExprPtr e)    { return std::make_unique<Stmt>(Stmt{PrintStmt{std::move(e)}}); }

} // namespace lumen
