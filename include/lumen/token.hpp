#pragma once
#include <string>
#include <string_view>

namespace lumen {

// ── Token types ───────────────────────────────────────────────────────────────
enum class TT {
    // Single-char punctuation
    LPAREN, RPAREN, LBRACE, RBRACE,
    COMMA, SEMICOLON, DOT,
    PLUS, MINUS, STAR, SLASH, PERCENT,

    // One-or-two char operators
    BANG, BANG_EQ,
    EQ, EQ_EQ,
    LT, LT_EQ,
    GT, GT_EQ,

    // Literals
    IDENT, STRING, NUMBER,

    // Keywords
    AND, OR, NOT,
    IF, ELSE,
    WHILE, FOR,
    FN, RETURN,
    VAR,
    TRUE_KW, FALSE_KW, NIL_KW,
    PRINT,

    // Meta
    EOF_T, ERROR,
};

// ── Token ─────────────────────────────────────────────────────────────────────
struct Token {
    TT          type{TT::ERROR};
    std::string lexeme;
    int         line{0};

    bool is(TT t)  const noexcept { return type == t; }
    bool isNot(TT t) const noexcept { return type != t; }
    bool isEof() const noexcept { return type == TT::EOF_T; }
};

// Human-readable name (for error messages)
std::string_view ttName(TT t) noexcept;

} // namespace lumen
