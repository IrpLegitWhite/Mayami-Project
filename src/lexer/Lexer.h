// src/lexer/Lexer.h
#pragma once

#include "Token.h"
#include <string>
#include <vector>

namespace mayami {

    class Lexer {
    public:
        Lexer(const std::string& source);

        std::vector<Token> tokenize();

        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

    private:
        char peek(int offset = 0) const;
        char advance();
        bool isAtEnd() const;

        void skipWhitespace();
        void skipLineComment();
        void skipBlockComment();

        Token makeToken(TokenType type);
        Token makeToken(TokenType type, const std::string& lexeme);

        Token scanNumber();
        Token scanString(char quote);
        Token scanIdentifier();

        void error(const std::string& msg);

        const std::string& m_source;
        size_t m_pos = 0;
        int m_line = 1;
        int m_col = 1;
        std::vector<std::string> m_errors;
    };

} // namespace mayami