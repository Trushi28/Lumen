#include "lumen/token.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <stdexcept>

namespace lumen {

std::string_view ttName(TT t) noexcept {
    switch (t) {
        case TT::LPAREN:   return "(";
        case TT::RPAREN:   return ")";
        case TT::LBRACE:   return "{";
        case TT::RBRACE:   return "}";
        case TT::COMMA:    return ",";
        case TT::SEMICOLON:return ";";
        case TT::PLUS:     return "+";
        case TT::MINUS:    return "-";
        case TT::STAR:     return "*";
        case TT::SLASH:    return "/";
        case TT::PERCENT:  return "%";
        case TT::BANG:     return "!";
        case TT::BANG_EQ:  return "!=";
        case TT::EQ:       return "=";
        case TT::EQ_EQ:    return "==";
        case TT::LT:       return "<";
        case TT::LT_EQ:    return "<=";
        case TT::GT:       return ">";
        case TT::GT_EQ:    return ">=";
        case TT::IDENT:    return "<ident>";
        case TT::STRING:   return "<string>";
        case TT::NUMBER:   return "<number>";
        case TT::AND:      return "and";
        case TT::OR:       return "or";
        case TT::NOT:      return "not";
        case TT::IF:       return "if";
        case TT::ELSE:     return "else";
        case TT::WHILE:    return "while";
        case TT::FOR:      return "for";
        case TT::FN:       return "fn";
        case TT::RETURN:   return "return";
        case TT::VAR:      return "var";
        case TT::TRUE_KW:  return "true";
        case TT::FALSE_KW: return "false";
        case TT::NIL_KW:   return "nil";
        case TT::PRINT:    return "print";
        case TT::EOF_T:    return "<eof>";
        default:           return "<error>";
    }
}

// ── Lexer class (local to this TU) ───────────────────────────────────────────
class Lexer {
  public:
    explicit Lexer(std::string source) : _src(std::move(source)) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (!atEnd()) {
            _start = _pos;
            auto tok = nextToken();
            if (tok.type != TT::ERROR || !tok.lexeme.empty())
                tokens.push_back(std::move(tok));
        }
        tokens.push_back(makeToken(TT::EOF_T, ""));
        return tokens;
    }

  private:
    std::string _src;
    size_t _pos{0}, _start{0};
    int    _line{1};

    static const std::unordered_map<std::string, TT> _keywords;

    bool atEnd()       const { return _pos >= _src.size(); }
    char peek()        const { return atEnd() ? '\0' : _src[_pos]; }
    char peekNext()    const { return (_pos + 1 >= _src.size()) ? '\0' : _src[_pos + 1]; }
    char advance()           { return _src[_pos++]; }

    bool match(char c) {
        if (atEnd() || _src[_pos] != c) return false;
        ++_pos; return true;
    }

    Token makeToken(TT type, std::string lex = {}) {
        if (lex.empty()) lex = _src.substr(_start, _pos - _start);
        return {type, std::move(lex), _line};
    }

    Token errorToken(const std::string& msg) {
        return {TT::ERROR, msg, _line};
    }

    void skipWhitespaceAndComments() {
        while (!atEnd()) {
            char c = peek();
            if (c == ' ' || c == '\r' || c == '\t') { advance(); }
            else if (c == '\n') { ++_line; advance(); }
            else if (c == '/' && peekNext() == '/') {
                while (!atEnd() && peek() != '\n') advance();
            }
            else break;
        }
    }

    Token lexString() {
        while (!atEnd() && peek() != '"') {
            if (peek() == '\n') ++_line;
            advance();
        }
        if (atEnd()) return errorToken("Unterminated string");
        advance(); // closing "
        // Extract without the quotes
        std::string value = _src.substr(_start + 1, _pos - _start - 2);
        return {TT::STRING, std::move(value), _line};
    }

    Token lexNumber() {
        while (!atEnd() && std::isdigit(peek())) advance();
        if (!atEnd() && peek() == '.' && std::isdigit(peekNext())) {
            advance(); // consume '.'
            while (!atEnd() && std::isdigit(peek())) advance();
        }
        return makeToken(TT::NUMBER);
    }

    Token lexIdent() {
        while (!atEnd() && (std::isalnum(peek()) || peek() == '_')) advance();
        std::string lex = _src.substr(_start, _pos - _start);
        auto it = _keywords.find(lex);
        TT type = (it != _keywords.end()) ? it->second : TT::IDENT;
        return {type, std::move(lex), _line};
    }

    Token nextToken() {
        skipWhitespaceAndComments();
        _start = _pos;
        if (atEnd()) return makeToken(TT::EOF_T, "");

        char c = advance();
        switch (c) {
            case '(': return makeToken(TT::LPAREN);
            case ')': return makeToken(TT::RPAREN);
            case '{': return makeToken(TT::LBRACE);
            case '}': return makeToken(TT::RBRACE);
            case ',': return makeToken(TT::COMMA);
            case ';': return makeToken(TT::SEMICOLON);
            case '+': return makeToken(TT::PLUS);
            case '-': return makeToken(TT::MINUS);
            case '*': return makeToken(TT::STAR);
            case '/': return makeToken(TT::SLASH);
            case '%': return makeToken(TT::PERCENT);
            case '!': return makeToken(match('=') ? TT::BANG_EQ : TT::BANG);
            case '=': return makeToken(match('=') ? TT::EQ_EQ   : TT::EQ);
            case '<': return makeToken(match('=') ? TT::LT_EQ   : TT::LT);
            case '>': return makeToken(match('=') ? TT::GT_EQ   : TT::GT);
            case '"': return lexString();
            default:
                if (std::isdigit(c)) return lexNumber();
                if (std::isalpha(c) || c == '_') return lexIdent();
                return errorToken(std::string("Unexpected character: ") + c);
        }
    }
};

const std::unordered_map<std::string, TT> Lexer::_keywords = {
    {"and",    TT::AND},   {"or",     TT::OR},    {"not",    TT::NOT},
    {"if",     TT::IF},    {"else",   TT::ELSE},
    {"while",  TT::WHILE}, {"for",    TT::FOR},
    {"fn",     TT::FN},    {"return", TT::RETURN},
    {"var",    TT::VAR},
    {"true",   TT::TRUE_KW}, {"false", TT::FALSE_KW}, {"nil", TT::NIL_KW},
    {"print",  TT::PRINT},
};

// Public function used by parser
std::vector<Token> tokenize(const std::string& source) {
    return Lexer(source).tokenize();
}

} // namespace lumen
