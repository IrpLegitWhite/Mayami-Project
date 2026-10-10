// src/lexer/Lexer.cpp
#include "Lexer.h"
#include <unordered_map>
#include <cctype>
#include <cstdio>

namespace mayami {

    // ============================================================
    // ★ Таблица ключевых слов (полные + короткие алиасы)
    // ============================================================
    static const std::unordered_map<std::string, TokenType> KEYWORDS = {
        // ─── Полные ключевые слова ──────────────────────────
        { "fn",       TokenType::KwFn },
        { "let",      TokenType::KwLet },
        { "mut",      TokenType::KwMut },
        { "if",       TokenType::KwIf },
        { "else",     TokenType::KwElse },
        { "while",    TokenType::KwWhile },
        { "for",      TokenType::KwFor },
        { "loop",     TokenType::KwLoop },
        { "break",    TokenType::KwBreak },
        { "continue", TokenType::KwContinue },
        { "return",   TokenType::KwReturn },
        { "struct",   TokenType::KwStruct },
        { "enum",     TokenType::KwEnum },
        { "trait",    TokenType::KwTrait },
        { "impl",     TokenType::KwImpl },
        { "match",    TokenType::KwMatch },
        { "async",    TokenType::KwAsync },
        { "await",    TokenType::KwAwait },
        { "pub",      TokenType::KwPub },
        { "use",      TokenType::KwUse },
        { "mod",      TokenType::KwMod },
        { "const",    TokenType::KwConst },
        { "static",   TokenType::KwStatic },
        { "in",       TokenType::KwIn },
        { "as",       TokenType::KwAs },
        { "self",     TokenType::KwSelf },
        { "Self",     TokenType::KwSelfType },
        { "where",    TokenType::KwWhere },

        // Типы (полные)
        { "i8",   TokenType::KwI8},   { "i16",  TokenType::KwI16},
        { "i32",  TokenType::KwI32},  { "i64",  TokenType::KwI64},
        { "u8",   TokenType::KwU8},   { "u16",  TokenType::KwU16},
        { "u32",  TokenType::KwU32},  { "u64",  TokenType::KwU64},
        { "f32",  TokenType::KwF32},  { "f64",  TokenType::KwF64},
        { "bool", TokenType::KwBool}, { "string", TokenType::KwString},
        { "char", TokenType::KwChar}, { "void", TokenType::KwVoid},

        // Литералы
        { "true",  TokenType::True},
        { "false", TokenType::False},

        // ─── ★ СУПЕР-КОРОТКИЕ АЛИАСЫ ───────────────────────
        { "f",   TokenType::KwFn },         // fn
        { "l",   TokenType::KwLet },        // let
        { "m",   TokenType::KwMut },        // mut
        { "el",  TokenType::KwElse },       // else
        { "wh",  TokenType::KwWhile },      // while
        { "fr",  TokenType::KwFor },        // for
        { "lp",  TokenType::KwLoop },       // loop
        { "br",  TokenType::KwBreak },      // break
        { "cn",  TokenType::KwContinue },   // continue
        { "r",   TokenType::KwReturn },     // return
        { "st",  TokenType::KwStruct },     // struct
        { "en",  TokenType::KwEnum },       // enum
        { "tr",  TokenType::KwTrait },      // trait
        { "im",  TokenType::KwImpl },       // impl
        { "mt",  TokenType::KwMatch },      // match
        { "t",   TokenType::True },         // true
        { "F",   TokenType::False },        // false
    };

    // ============================================================
    // tokenTypeName
    // ============================================================
    std::string tokenTypeName(TokenType type) {
        switch (type) {
        case TokenType::EndOfFile:   return "EOF";
        case TokenType::Unknown:     return "UNKNOWN";
        case TokenType::Integer:     return "INT";
        case TokenType::Float:       return "FLOAT";
        case TokenType::String:      return "STRING";
        case TokenType::Char:        return "CHAR";
        case TokenType::True:        return "TRUE";
        case TokenType::False:       return "FALSE";
        case TokenType::Identifier:  return "IDENT";
        case TokenType::KwFn:        return "KW_FN";
        case TokenType::KwLet:       return "KW_LET";
        case TokenType::KwMut:       return "KW_MUT";
        case TokenType::KwIf:        return "KW_IF";
        case TokenType::KwElse:      return "KW_ELSE";
        case TokenType::KwWhile:     return "KW_WHILE";
        case TokenType::KwFor:       return "KW_FOR";
        case TokenType::KwLoop:      return "KW_LOOP";
        case TokenType::KwBreak:     return "KW_BREAK";
        case TokenType::KwContinue:  return "KW_CONTINUE";
        case TokenType::KwReturn:    return "KW_RETURN";
        case TokenType::KwStruct:    return "KW_STRUCT";
        case TokenType::KwEnum:      return "KW_ENUM";
        case TokenType::KwTrait:     return "KW_TRAIT";
        case TokenType::KwImpl:      return "KW_IMPL";
        case TokenType::KwMatch:     return "KW_MATCH";
        case TokenType::KwAsync:     return "KW_ASYNC";
        case TokenType::KwAwait:     return "KW_AWAIT";
        case TokenType::KwPub:       return "KW_PUB";
        case TokenType::KwUse:       return "KW_USE";
        case TokenType::KwMod:       return "KW_MOD";
        case TokenType::KwConst:     return "KW_CONST";
        case TokenType::KwStatic:    return "KW_STATIC";
        case TokenType::KwIn:        return "KW_IN";
        case TokenType::KwAs:        return "KW_AS";
        case TokenType::KwSelf:      return "KW_SELF";
        case TokenType::KwSelfType:  return "KW_SELFTYPE";
        case TokenType::KwWhere:     return "KW_WHERE";
        case TokenType::KwI8:        return "TYPE_I8";
        case TokenType::KwI16:       return "TYPE_I16";
        case TokenType::KwI32:       return "TYPE_I32";
        case TokenType::KwI64:       return "TYPE_I64";
        case TokenType::KwU8:        return "TYPE_U8";
        case TokenType::KwU16:       return "TYPE_U16";
        case TokenType::KwU32:       return "TYPE_U32";
        case TokenType::KwU64:       return "TYPE_U64";
        case TokenType::KwF32:       return "TYPE_F32";
        case TokenType::KwF64:       return "TYPE_F64";
        case TokenType::KwBool:      return "TYPE_BOOL";
        case TokenType::KwString:    return "TYPE_STRING";
        case TokenType::KwChar:      return "TYPE_CHAR";
        case TokenType::KwVoid:      return "TYPE_VOID";
        case TokenType::Plus:        return "PLUS";
        case TokenType::Minus:       return "MINUS";
        case TokenType::Star:        return "STAR";
        case TokenType::Slash:       return "SLASH";
        case TokenType::Percent:     return "PERCENT";
        case TokenType::Assign:      return "ASSIGN";
        case TokenType::Eq:          return "EQ";
        case TokenType::NotEq:       return "NOTEQ";
        case TokenType::Lt:          return "LT";
        case TokenType::Gt:          return "GT";
        case TokenType::LtEq:        return "LTEQ";
        case TokenType::GtEq:        return "GTEQ";
        case TokenType::And:         return "AND";
        case TokenType::Or:          return "OR";
        case TokenType::Not:         return "NOT";
        case TokenType::Amp:         return "AMP";
        case TokenType::Pipe:        return "PIPE";
        case TokenType::Caret:       return "CARET";
        case TokenType::Tilde:       return "TILDE";
        case TokenType::Shl:         return "SHL";
        case TokenType::Shr:         return "SHR";
        case TokenType::PlusEq:      return "PLUSEQ";
        case TokenType::MinusEq:     return "MINUSEQ";
        case TokenType::StarEq:      return "STAREQ";
        case TokenType::SlashEq:     return "SLASHEQ";
        case TokenType::PercentEq:   return "PERCENTEQ";
        case TokenType::AmpEq:       return "AMPEQ";
        case TokenType::PipeEq:      return "PIPEEQ";
        case TokenType::CaretEq:     return "CARETEQ";
        case TokenType::ShlEq:       return "SHLEQ";
        case TokenType::ShrEq:       return "SHREQ";
        case TokenType::Arrow:       return "ARROW";
        case TokenType::FatArrow:    return "FATARROW";
        case TokenType::ColonColon:  return "COLONCOLON";
        case TokenType::Dot:         return "DOT";
        case TokenType::DotDot:      return "DOTDOT";
        case TokenType::DotDotEq:    return "DOTDOTEQ";
        case TokenType::Question:    return "QUESTION";
        case TokenType::LParen:      return "LPAREN";
        case TokenType::RParen:      return "RPAREN";
        case TokenType::LBrace:      return "LBRACE";
        case TokenType::RBrace:      return "RBRACE";
        case TokenType::LBracket:    return "LBRACKET";
        case TokenType::RBracket:    return "RBRACKET";
        case TokenType::Comma:       return "COMMA";
        case TokenType::Semi:        return "SEMI";
        case TokenType::Colon:       return "COLON";
        }
        return "?";
    }

    // ============================================================
    // Lexer
    // ============================================================
    Lexer::Lexer(const std::string& source) : m_source(source) {}

    char Lexer::peek(int offset) const {
        size_t p = m_pos + offset;
        return (p < m_source.size()) ? m_source[p] : '\0';
    }

    char Lexer::advance() {
        if (isAtEnd()) return '\0';
        char c = m_source[m_pos++];
        if (c == '\n') { m_line++; m_col = 1; }
        else { m_col++; }
        return c;
    }

    bool Lexer::isAtEnd() const {
        return m_pos >= m_source.size();
    }

    void Lexer::skipWhitespace() {
        while (!isAtEnd()) {
            char c = peek();
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
            }
            else if (c == '/' && peek(1) == '/') {
                skipLineComment();
            }
            else if (c == '/' && peek(1) == '*') {
                skipBlockComment();
            }
            else {
                break;
            }
        }
    }

    void Lexer::skipLineComment() {
        while (!isAtEnd() && peek() != '\n') advance();
    }

    void Lexer::skipBlockComment() {
        advance();
        advance();
        while (!isAtEnd()) {
            if (peek() == '*' && peek(1) == '/') {
                advance(); advance();
                return;
            }
            advance();
        }
        error("Unterminated block comment");
    }

    Token Lexer::makeToken(TokenType type) {
        return Token(type, "", m_line, m_col);
    }

    Token Lexer::makeToken(TokenType type, const std::string& lexeme) {
        return Token(type, lexeme, m_line, m_col);
    }

    void Lexer::error(const std::string& msg) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Lexer error at line %d, col %d: %s",
            m_line, m_col, msg.c_str());
        m_errors.push_back(buf);
    }

    Token Lexer::scanNumber() {
        int startLine = m_line, startCol = m_col;
        std::string num;

        while (!isAtEnd() && std::isdigit((unsigned char)peek()))
            num += advance();

        bool isFloat = false;
        if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
            isFloat = true;
            num += advance();
            while (!isAtEnd() && std::isdigit((unsigned char)peek()))
                num += advance();
        }

        return Token(isFloat ? TokenType::Float : TokenType::Integer,
            num, startLine, startCol);
    }

    Token Lexer::scanString(char quote) {
        int startLine = m_line, startCol = m_col;
        advance();

        std::string value;
        while (!isAtEnd() && peek() != quote) {
            if (peek() == '\\' && peek(1) != '\0') {
                advance();
                char esc = advance();
                switch (esc) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"':  value += '"';  break;
                case '\'': value += '\''; break;
                case '0':  value += '\0'; break;
                default:   value += esc;  break;
                }
            }
            else {
                value += advance();
            }
        }

        if (isAtEnd()) {
            error("Unterminated string literal");
        }
        else {
            advance();
        }

        return Token(quote == '"' ? TokenType::String : TokenType::Char,
            value, startLine, startCol);
    }

    Token Lexer::scanIdentifier() {
        int startLine = m_line, startCol = m_col;
        std::string id;

        while (!isAtEnd() &&
            (std::isalnum((unsigned char)peek()) || peek() == '_')) {
            id += advance();
        }

        auto it = KEYWORDS.find(id);
        if (it != KEYWORDS.end()) {
            return Token(it->second, id, startLine, startCol);
        }

        return Token(TokenType::Identifier, id, startLine, startCol);
    }

    std::vector<Token> Lexer::tokenize() {
        std::vector<Token> tokens;

        while (true) {
            skipWhitespace();
            if (isAtEnd()) {
                tokens.push_back(makeToken(TokenType::EndOfFile, ""));
                break;
            }

            char c = peek();

            if (std::isdigit((unsigned char)c)) {
                tokens.push_back(scanNumber());
                continue;
            }

            if (c == '"' || c == '\'') {
                tokens.push_back(scanString(c));
                continue;
            }

            if (std::isalpha((unsigned char)c) || c == '_') {
                tokens.push_back(scanIdentifier());
                continue;
            }

            int line = m_line, col = m_col;

            auto two = [&](char c1, char c2) {
                return peek() == c1 && peek(1) == c2;
                };
            auto three = [&](char c1, char c2, char c3) {
                return peek() == c1 && peek(1) == c2 && peek(2) == c3;
                };

            if (three('<', '<', '=')) { advance(); advance(); advance(); tokens.push_back(Token(TokenType::ShlEq, "<<=", line, col)); continue; }
            if (three('>', '>', '=')) { advance(); advance(); advance(); tokens.push_back(Token(TokenType::ShrEq, ">>=", line, col)); continue; }
            if (three('.', '.', '=')) { advance(); advance(); advance(); tokens.push_back(Token(TokenType::DotDotEq, "..=", line, col)); continue; }

            if (two('=', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::Eq, "==", line, col)); continue; }
            if (two('!', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::NotEq, "!=", line, col)); continue; }
            if (two('<', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::LtEq, "<=", line, col)); continue; }
            if (two('>', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::GtEq, ">=", line, col)); continue; }
            if (two('&', '&')) { advance(); advance(); tokens.push_back(Token(TokenType::And, "&&", line, col)); continue; }
            if (two('|', '|')) { advance(); advance(); tokens.push_back(Token(TokenType::Or, "||", line, col)); continue; }
            if (two('<', '<')) { advance(); advance(); tokens.push_back(Token(TokenType::Shl, "<<", line, col)); continue; }
            if (two('>', '>')) { advance(); advance(); tokens.push_back(Token(TokenType::Shr, ">>", line, col)); continue; }
            if (two('+', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::PlusEq, "+=", line, col)); continue; }
            if (two('-', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::MinusEq, "-=", line, col)); continue; }
            if (two('*', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::StarEq, "*=", line, col)); continue; }
            if (two('/', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::SlashEq, "/=", line, col)); continue; }
            if (two('%', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::PercentEq, "%=", line, col)); continue; }
            if (two('&', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::AmpEq, "&=", line, col)); continue; }
            if (two('|', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::PipeEq, "|=", line, col)); continue; }
            if (two('^', '=')) { advance(); advance(); tokens.push_back(Token(TokenType::CaretEq, "^=", line, col)); continue; }
            if (two('-', '>')) { advance(); advance(); tokens.push_back(Token(TokenType::Arrow, "->", line, col)); continue; }
            if (two('=', '>')) { advance(); advance(); tokens.push_back(Token(TokenType::FatArrow, "=>", line, col)); continue; }
            if (two(':', ':')) { advance(); advance(); tokens.push_back(Token(TokenType::ColonColon, "::", line, col)); continue; }
            if (two('.', '.')) { advance(); advance(); tokens.push_back(Token(TokenType::DotDot, "..", line, col)); continue; }

            switch (c) {
            case '+': advance(); tokens.push_back(Token(TokenType::Plus, "+", line, col)); break;
            case '-': advance(); tokens.push_back(Token(TokenType::Minus, "-", line, col)); break;
            case '*': advance(); tokens.push_back(Token(TokenType::Star, "*", line, col)); break;
            case '/': advance(); tokens.push_back(Token(TokenType::Slash, "/", line, col)); break;
            case '%': advance(); tokens.push_back(Token(TokenType::Percent, "%", line, col)); break;
            case '=': advance(); tokens.push_back(Token(TokenType::Assign, "=", line, col)); break;
            case '<': advance(); tokens.push_back(Token(TokenType::Lt, "<", line, col)); break;
            case '>': advance(); tokens.push_back(Token(TokenType::Gt, ">", line, col)); break;
            case '!': advance(); tokens.push_back(Token(TokenType::Not, "!", line, col)); break;
            case '&': advance(); tokens.push_back(Token(TokenType::Amp, "&", line, col)); break;
            case '|': advance(); tokens.push_back(Token(TokenType::Pipe, "|", line, col)); break;
            case '^': advance(); tokens.push_back(Token(TokenType::Caret, "^", line, col)); break;
            case '~': advance(); tokens.push_back(Token(TokenType::Tilde, "~", line, col)); break;
            case '.': advance(); tokens.push_back(Token(TokenType::Dot, ".", line, col)); break;
            case '?': advance(); tokens.push_back(Token(TokenType::Question, "?", line, col)); break;
            case '(': advance(); tokens.push_back(Token(TokenType::LParen, "(", line, col)); break;
            case ')': advance(); tokens.push_back(Token(TokenType::RParen, ")", line, col)); break;
            case '{': advance(); tokens.push_back(Token(TokenType::LBrace, "{", line, col)); break;
            case '}': advance(); tokens.push_back(Token(TokenType::RBrace, "}", line, col)); break;
            case '[': advance(); tokens.push_back(Token(TokenType::LBracket, "[", line, col)); break;
            case ']': advance(); tokens.push_back(Token(TokenType::RBracket, "]", line, col)); break;
            case ',': advance(); tokens.push_back(Token(TokenType::Comma, ",", line, col)); break;
            case ';': advance(); tokens.push_back(Token(TokenType::Semi, ";", line, col)); break;
            case ':': advance(); tokens.push_back(Token(TokenType::Colon, ":", line, col)); break;
            default:
                error(std::string("Unexpected character '") + c + "'");
                advance();
                break;
            }
        }

        return tokens;
    }

} // namespace mayami