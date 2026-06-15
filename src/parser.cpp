#include "lumen/ast.hpp"
#include "lumen/token.hpp"
#include <sstream>
#include <stdexcept>
#include <vector>

namespace lumen {

// Forward declaration from lexer.cpp
std::vector<Token> tokenize(const std::string& source);

// ── Parse error ───────────────────────────────────────────────────────────────
struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ── Parser ────────────────────────────────────────────────────────────────────
class Parser {
  public:
    explicit Parser(std::vector<Token> tokens) : _tokens(std::move(tokens)) {}

    std::vector<StmtPtr> parse() {
        std::vector<StmtPtr> stmts;
        while (!check(TT::EOF_T)) {
            stmts.push_back(declaration());
        }
        return stmts;
    }

  private:
    std::vector<Token> _tokens;
    size_t             _pos{0};

    // ── Token navigation ──────────────────────────────────────────────────────
    Token& current()       { return _tokens[_pos]; }
    Token& previous()      { return _tokens[_pos - 1]; }
    bool   atEnd()   const { return _tokens[_pos].type == TT::EOF_T; }

    bool check(TT t)  const { return _tokens[_pos].type == t; }
    Token& advance()        { if (!atEnd()) ++_pos; return previous(); }

    bool match(std::initializer_list<TT> types) {
        for (TT t : types) {
            if (check(t)) { advance(); return true; }
        }
        return false;
    }

    Token& consume(TT t, const std::string& msg) {
        if (check(t)) return advance();
        throw ParseError(std::to_string(current().line) + ": " + msg
                         + " (got '" + current().lexeme + "')");
    }

    // ── Synchronise after error ───────────────────────────────────────────────
    void synchronise() {
        advance();
        while (!atEnd()) {
            if (previous().type == TT::SEMICOLON) return;
            switch (current().type) {
                case TT::FN: case TT::VAR: case TT::IF:
                case TT::WHILE: case TT::FOR: case TT::RETURN: case TT::PRINT:
                    return;
                default: advance();
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    // Statements
    // ══════════════════════════════════════════════════════════════════════════

    StmtPtr declaration() {
        try {
            if (match({TT::VAR}))  return varDecl();
            if (match({TT::FN}))   return fnDecl("function");
            return statement();
        } catch (const ParseError&) {
            synchronise();
            return nullptr;
        }
    }

    StmtPtr varDecl() {
        Token name = consume(TT::IDENT, "Expected variable name");
        ExprPtr init;
        if (match({TT::EQ})) init = expression();
        consume(TT::SEMICOLON, "Expected ';' after variable declaration");
        auto s = std::make_unique<Stmt>();
        s->v = VarDecl{std::move(name), std::move(init)};
        return s;
    }

    StmtPtr fnDecl(const std::string& kind) {
        Token name = consume(TT::IDENT, "Expected " + kind + " name");
        consume(TT::LPAREN, "Expected '(' after " + kind + " name");

        std::vector<Token> params;
        if (!check(TT::RPAREN)) {
            do {
                if (params.size() >= 255)
                    throw ParseError(std::to_string(current().line) + ": Too many parameters");
                params.push_back(consume(TT::IDENT, "Expected parameter name"));
            } while (match({TT::COMMA}));
        }
        consume(TT::RPAREN, "Expected ')' after parameters");
        consume(TT::LBRACE, "Expected '{' before " + kind + " body");

        std::vector<StmtPtr> body;
        while (!check(TT::RBRACE) && !atEnd()) body.push_back(declaration());
        consume(TT::RBRACE, "Expected '}' after " + kind + " body");

        auto s = std::make_unique<Stmt>();
        s->v = FnDecl{std::move(name), std::move(params), std::move(body)};
        return s;
    }

    StmtPtr statement() {
        if (match({TT::PRINT}))  return printStmt();
        if (match({TT::IF}))     return ifStmt();
        if (match({TT::WHILE}))  return whileStmt();
        if (match({TT::FOR}))    return forStmt();
        if (match({TT::RETURN})) return returnStmt();
        if (match({TT::LBRACE})) return block();
        return exprStmt();
    }

    StmtPtr printStmt() {
        ExprPtr val = expression();
        consume(TT::SEMICOLON, "Expected ';' after value");
        return makePrintStmt(std::move(val));
    }

    StmtPtr exprStmt() {
        ExprPtr e = expression();
        consume(TT::SEMICOLON, "Expected ';' after expression");
        return makeExprStmt(std::move(e));
    }

    StmtPtr block() {
        std::vector<StmtPtr> stmts;
        while (!check(TT::RBRACE) && !atEnd()) stmts.push_back(declaration());
        consume(TT::RBRACE, "Expected '}' to close block");
        auto s = std::make_unique<Stmt>();
        s->v = BlockStmt{std::move(stmts)};
        return s;
    }

    StmtPtr ifStmt() {
        consume(TT::LPAREN, "Expected '(' after 'if'");
        ExprPtr cond = expression();
        consume(TT::RPAREN, "Expected ')' after if condition");
        StmtPtr then_br = statement();
        StmtPtr else_br;
        if (match({TT::ELSE})) else_br = statement();
        auto s = std::make_unique<Stmt>();
        s->v = IfStmt{std::move(cond), std::move(then_br), std::move(else_br)};
        return s;
    }

    StmtPtr whileStmt() {
        consume(TT::LPAREN, "Expected '(' after 'while'");
        ExprPtr cond = expression();
        consume(TT::RPAREN, "Expected ')' after condition");
        StmtPtr body = statement();
        auto s = std::make_unique<Stmt>();
        s->v = WhileStmt{std::move(cond), std::move(body)};
        return s;
    }

    // Desugars `for (init; cond; inc)` into a while loop
    StmtPtr forStmt() {
        consume(TT::LPAREN, "Expected '(' after 'for'");

        StmtPtr init;
        if (match({TT::SEMICOLON})) { /* no init */ }
        else if (match({TT::VAR})) init = varDecl();
        else init = exprStmt();

        ExprPtr cond;
        if (!check(TT::SEMICOLON)) cond = expression();
        consume(TT::SEMICOLON, "Expected ';' after for condition");

        ExprPtr inc;
        if (!check(TT::RPAREN)) inc = expression();
        consume(TT::RPAREN, "Expected ')' after for clauses");

        StmtPtr body = statement();

        // Append increment to body
        if (inc) {
            std::vector<StmtPtr> stmts;
            stmts.push_back(std::move(body));
            stmts.push_back(makeExprStmt(std::move(inc)));
            auto blk = std::make_unique<Stmt>();
            blk->v = BlockStmt{std::move(stmts)};
            body = std::move(blk);
        }

        // Wrap in while
        if (!cond) {
            Token trueTok{TT::TRUE_KW, "true", current().line};
            cond = makeLiteral(std::move(trueTok));
        }
        auto wh = std::make_unique<Stmt>();
        wh->v = WhileStmt{std::move(cond), std::move(body)};

        // Wrap initialiser
        if (init) {
            std::vector<StmtPtr> stmts;
            stmts.push_back(std::move(init));
            stmts.push_back(std::move(wh));
            auto blk = std::make_unique<Stmt>();
            blk->v = BlockStmt{std::move(stmts)};
            return blk;
        }
        return wh;
    }

    StmtPtr returnStmt() {
        Token kw = previous();
        ExprPtr val;
        if (!check(TT::SEMICOLON)) val = expression();
        consume(TT::SEMICOLON, "Expected ';' after return value");
        auto s = std::make_unique<Stmt>();
        s->v = ReturnStmt{std::move(kw), std::move(val)};
        return s;
    }

    // ══════════════════════════════════════════════════════════════════════════
    // Expressions  (Pratt-style precedence climbing via recursive descent)
    // ══════════════════════════════════════════════════════════════════════════

    ExprPtr expression() { return assignment(); }

    ExprPtr assignment() {
        ExprPtr expr = logicalOr();
        if (match({TT::EQ})) {
            // Check that LHS is a simple variable
            if (auto* ve = std::get_if<VarExpr>(&expr->v)) {
                Token name = ve->name;
                ExprPtr val = assignment();
                return makeAssign(std::move(name), std::move(val));
            }
            throw ParseError(std::to_string(previous().line) + ": Invalid assignment target");
        }
        return expr;
    }

    ExprPtr logicalOr() {
        ExprPtr left = logicalAnd();
        while (match({TT::OR})) {
            Token op = previous();
            ExprPtr right = logicalAnd();
            left = makeLogical(std::move(left), std::move(op), std::move(right));
        }
        return left;
    }

    ExprPtr logicalAnd() {
        ExprPtr left = equality();
        while (match({TT::AND})) {
            Token op = previous();
            ExprPtr right = equality();
            left = makeLogical(std::move(left), std::move(op), std::move(right));
        }
        return left;
    }

    ExprPtr equality() {
        ExprPtr left = comparison();
        while (match({TT::EQ_EQ, TT::BANG_EQ})) {
            Token op = previous();
            left = makeBinary(std::move(left), std::move(op), comparison());
        }
        return left;
    }

    ExprPtr comparison() {
        ExprPtr left = term();
        while (match({TT::LT, TT::LT_EQ, TT::GT, TT::GT_EQ})) {
            Token op = previous();
            left = makeBinary(std::move(left), std::move(op), term());
        }
        return left;
    }

    ExprPtr term() {
        ExprPtr left = factor();
        while (match({TT::PLUS, TT::MINUS})) {
            Token op = previous();
            left = makeBinary(std::move(left), std::move(op), factor());
        }
        return left;
    }

    ExprPtr factor() {
        ExprPtr left = unary();
        while (match({TT::STAR, TT::SLASH, TT::PERCENT})) {
            Token op = previous();
            left = makeBinary(std::move(left), std::move(op), unary());
        }
        return left;
    }

    ExprPtr unary() {
        if (match({TT::BANG, TT::MINUS, TT::NOT})) {
            Token op = previous();
            return makeUnary(std::move(op), unary());
        }
        return call();
    }

    ExprPtr call() {
        ExprPtr expr = primary();
        while (true) {
            if (!match({TT::LPAREN})) break;
            Token paren = previous();
            std::vector<ExprPtr> args;
            if (!check(TT::RPAREN)) {
                do {
                    if (args.size() >= 255)
                        throw ParseError(std::to_string(current().line) + ": Too many arguments");
                    args.push_back(expression());
                } while (match({TT::COMMA}));
            }
            consume(TT::RPAREN, "Expected ')' after arguments");
            expr = makeCall(std::move(expr), std::move(paren), std::move(args));
        }
        return expr;
    }

    ExprPtr primary() {
        if (match({TT::NIL_KW, TT::TRUE_KW, TT::FALSE_KW, TT::NUMBER, TT::STRING}))
            return makeLiteral(previous());

        if (match({TT::IDENT}))
            return makeVar(previous());

        if (match({TT::LPAREN})) {
            ExprPtr e = expression();
            consume(TT::RPAREN, "Expected ')' after expression");
            return e; // grouping — no separate node needed
        }

        throw ParseError(std::to_string(current().line)
                         + ": Expected expression, got '" + current().lexeme + "'");
    }
};

// ── Public API ────────────────────────────────────────────────────────────────
std::vector<StmtPtr> parse(const std::string& source) {
    auto tokens = tokenize(source);
    return Parser(std::move(tokens)).parse();
}

// ── Expr::line() / Stmt::line() ───────────────────────────────────────────────
int Expr::line() const {
    return std::visit([](const auto& n) -> int {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, LiteralExpr>)  return n.tok.line;
        if constexpr (std::is_same_v<T, VarExpr>)      return n.name.line;
        if constexpr (std::is_same_v<T, AssignExpr>)   return n.name.line;
        if constexpr (std::is_same_v<T, UnaryExpr>)    return n.op.line;
        if constexpr (std::is_same_v<T, BinaryExpr>)   return n.op.line;
        if constexpr (std::is_same_v<T, LogicalExpr>)  return n.op.line;
        if constexpr (std::is_same_v<T, CallExpr>)     return n.paren.line;
        return 0;
    }, v);
}

int Stmt::line() const {
    return std::visit([](const auto& n) -> int {
        using T = std::decay_t<decltype(n)>;
        if constexpr (std::is_same_v<T, ExprStmt>)    return n.expr->line();
        if constexpr (std::is_same_v<T, PrintStmt>)   return n.expr->line();
        if constexpr (std::is_same_v<T, VarDecl>)     return n.name.line;
        if constexpr (std::is_same_v<T, FnDecl>)      return n.name.line;
        if constexpr (std::is_same_v<T, ReturnStmt>)  return n.keyword.line;
        if constexpr (std::is_same_v<T, BlockStmt>)   return n.body.empty() ? 0 : n.body.front()->line();
        if constexpr (std::is_same_v<T, IfStmt>)      return n.cond->line();
        if constexpr (std::is_same_v<T, WhileStmt>)   return n.cond->line();
        return 0;
    }, v);
}

} // namespace lumen
