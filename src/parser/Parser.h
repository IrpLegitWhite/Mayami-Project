// src/parser/Parser.h
#pragma once

#include "AST.h"
#include "../lexer/Token.h"
#include <vector>
#include <string>

namespace mayami {

    class Parser {
    public:
        Parser(const std::vector<Token>& tokens);
        std::unique_ptr<Program> parse();
        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

    private:
        const Token& peek(int offset = 0) const;
        const Token& previous() const;
        const Token& advance();
        bool isAtEnd() const;
        bool check(TokenType type) const;
        bool match(TokenType type);
        bool matchAny(std::initializer_list<TokenType> types);
        const Token& expect(TokenType type, const std::string& what);
        void error(const std::string& msg);
        void synchronize();

        DeclPtr parseDeclaration();
        DeclPtr parseFunctionDecl(bool pub = false);
        DeclPtr parseStructDecl(bool pub = false);
        DeclPtr parseEnumDecl(bool pub = false);
        DeclPtr parseImplDecl();
        DeclPtr parseUseDecl();
        DeclPtr parseImportDecl();   // ★

        bool isTopLevelStatement() const;

        StmtPtr parseStatement();
        StmtPtr parseLetStatement();
        StmtPtr parseIfStatement();
        StmtPtr parseWhileStatement();
        StmtPtr parseForStatement();
        StmtPtr parseLoopStatement();
        StmtPtr parseReturnStatement();
        StmtPtr parseBreakStatement();
        StmtPtr parseContinueStatement();
        std::unique_ptr<BlockStmt> parseBlock();

        ExprPtr parseExpression();
        ExprPtr parseAssignment();
        ExprPtr parseOr();
        ExprPtr parseAnd();
        ExprPtr parseEquality();
        ExprPtr parseComparison();
        ExprPtr parseAddition();
        ExprPtr parseMultiplication();
        ExprPtr parseUnary();
        ExprPtr parsePostfix();
        ExprPtr parsePrimary();

        TypePtr parseType();
        bool isTypeToken(TokenType type) const;

        const std::vector<Token>& m_tokens;
        size_t m_pos = 0;
        std::vector<std::string> m_errors;
    };

} // namespace mayami