// src/parser/Parser.cpp
#include "Parser.h"
#include <cstdio>

namespace mayami {

    Parser::Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {}

    // ─── Утилиты ───────────────────────────────────────────
    const Token& Parser::peek(int offset) const {
        size_t pos = m_pos + offset;
        if (pos >= m_tokens.size()) return m_tokens.back();
        return m_tokens[pos];
    }
    const Token& Parser::previous() const {
        return m_tokens[m_pos > 0 ? m_pos - 1 : 0];
    }
    const Token& Parser::advance() {
        if (!isAtEnd()) ++m_pos;
        return previous();
    }
    bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }
    bool Parser::check(TokenType type) const { return peek().type == type; }
    bool Parser::match(TokenType type) {
        if (check(type)) { advance(); return true; }
        return false;
    }
    bool Parser::matchAny(std::initializer_list<TokenType> types) {
        for (auto t : types) { if (check(t)) { advance(); return true; } }
        return false;
    }
    const Token& Parser::expect(TokenType type, const std::string& what) {
        if (check(type)) return advance();
        error("Expected " + what + " but got '" + peek().lexeme + "'");
        return peek();
    }
    void Parser::error(const std::string& msg) {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "Parser error at line %d, col %d: %s",
            peek().line, peek().column, msg.c_str());
        m_errors.push_back(buf);
    }
    void Parser::synchronize() {
        advance();
        while (!isAtEnd()) {
            if (previous().type == TokenType::Semi) return;
            switch (peek().type) {
            case TokenType::KwFn: case TokenType::KwLet: case TokenType::KwIf:
            case TokenType::KwWhile: case TokenType::KwFor: case TokenType::KwLoop:
            case TokenType::KwReturn: case TokenType::KwStruct: case TokenType::KwEnum:
            case TokenType::KwImpl: case TokenType::KwUse: case TokenType::RBrace:
                return;
            default: break;
            }
            advance();
        }
    }

    // ─── Top-level ─────────────────────────────────────────
    bool Parser::isTopLevelStatement() const {
        TokenType t = peek().type;
        switch (t) {
        case TokenType::KwLet: case TokenType::KwIf: case TokenType::KwWhile:
        case TokenType::KwFor: case TokenType::KwLoop: case TokenType::KwReturn:
        case TokenType::Question:
            return true;
        case TokenType::Identifier: case TokenType::KwSelf:
        case TokenType::Integer: case TokenType::Float: case TokenType::String:
        case TokenType::Char: case TokenType::True: case TokenType::False:
        case TokenType::LParen: case TokenType::LBracket:
        case TokenType::Minus: case TokenType::Not: case TokenType::Tilde:
            return true;
        default: return false;
        }
    }

    // ─── Types ─────────────────────────────────────────────
    bool Parser::isTypeToken(TokenType type) const {
        switch (type) {
        case TokenType::KwI8:  case TokenType::KwI16:
        case TokenType::KwI32: case TokenType::KwI64:
        case TokenType::KwU8:  case TokenType::KwU16:
        case TokenType::KwU32: case TokenType::KwU64:
        case TokenType::KwF32: case TokenType::KwF64:
        case TokenType::KwBool: case TokenType::KwString:
        case TokenType::KwChar: case TokenType::KwVoid:
            return true;
        default: return false;
        }
    }

    TypePtr Parser::parseType() {
        auto type = std::make_shared<Type>();
        if (match(TokenType::Amp)) {
            bool isMut = match(TokenType::KwMut);
            type->kind = isMut ? TypeKind::MutRef : TypeKind::Ref;
            type->inner = parseType();
            return type;
        }
        if (match(TokenType::Star)) {
            type->kind = TypeKind::Ptr;
            type->inner = parseType();
            return type;
        }
        if (match(TokenType::LBracket)) {
            auto inner = parseType();
            if (match(TokenType::Semi)) {
                type->kind = TypeKind::Array;
                type->inner = inner;
                if (check(TokenType::Integer)) {
                    type->arraySize = std::stoull(peek().lexeme);
                    advance();
                }
                expect(TokenType::RBracket, "']'");
            }
            else {
                type->kind = TypeKind::Slice;
                type->inner = inner;
                expect(TokenType::RBracket, "']'");
            }
            return type;
        }
        if (isTypeToken(peek().type)) {
            switch (advance().type) {
            case TokenType::KwI8:     type->kind = TypeKind::I8; break;
            case TokenType::KwI16:    type->kind = TypeKind::I16; break;
            case TokenType::KwI32:    type->kind = TypeKind::I32; break;
            case TokenType::KwI64:    type->kind = TypeKind::I64; break;
            case TokenType::KwU8:     type->kind = TypeKind::U8; break;
            case TokenType::KwU16:    type->kind = TypeKind::U16; break;
            case TokenType::KwU32:    type->kind = TypeKind::U32; break;
            case TokenType::KwU64:    type->kind = TypeKind::U64; break;
            case TokenType::KwF32:    type->kind = TypeKind::F32; break;
            case TokenType::KwF64:    type->kind = TypeKind::F64; break;
            case TokenType::KwBool:   type->kind = TypeKind::Bool; break;
            case TokenType::KwString: type->kind = TypeKind::String; break;
            case TokenType::KwChar:   type->kind = TypeKind::Char; break;
            case TokenType::KwVoid:   type->kind = TypeKind::Void; break;
            default: break;
            }
            return type;
        }
        if (check(TokenType::Identifier)) {
            type->kind = TypeKind::Named;
            type->name = advance().lexeme;
            return type;
        }
        error("Expected type");
        return type;
    }

    // ─── Program ───────────────────────────────────────────
    std::unique_ptr<Program> Parser::parse() {
        auto program = std::make_unique<Program>();
        std::vector<StmtPtr> topLevelStatements;

        while (!isAtEnd()) {
            try {
                if (isTopLevelStatement()) {
                    auto stmt = parseStatement();
                    if (stmt) topLevelStatements.push_back(std::move(stmt));
                    continue;
                }
                auto decl = parseDeclaration();
                if (decl) program->declarations.push_back(std::move(decl));
            }
            catch (...) { synchronize(); }
        }

        if (!topLevelStatements.empty()) {
            auto mainFn = std::make_unique<FunctionDecl>();
            mainFn->line = 1;
            mainFn->name = "main";
            auto body = std::make_unique<BlockStmt>();
            body->line = 1;
            body->statements = std::move(topLevelStatements);
            mainFn->body = std::move(body);
            program->declarations.insert(program->declarations.begin(),
                std::move(mainFn));
        }
        return program;
    }

    DeclPtr Parser::parseDeclaration() {
        bool isPub = match(TokenType::KwPub);
        if (check(TokenType::KwFn))     return parseFunctionDecl(isPub);
        if (check(TokenType::KwStruct)) return parseStructDecl(isPub);
        if (check(TokenType::KwEnum))   return parseEnumDecl(isPub);
        if (check(TokenType::KwImpl))   return parseImplDecl();
        if (check(TokenType::KwUse))    return parseUseDecl();
        error("Expected declaration (fn, struct, enum, impl, use)");
        synchronize();
        return nullptr;
    }

    DeclPtr Parser::parseFunctionDecl(bool isPub) {
        (void)isPub;
        auto fn = std::make_unique<FunctionDecl>();
        fn->line = peek().line;
        fn->column = peek().column;
        expect(TokenType::KwFn, "'fn'");
        fn->name = expect(TokenType::Identifier, "function name").lexeme;
        expect(TokenType::LParen, "'('");

        if (!check(TokenType::RParen)) {
            do {
                Param p;
                if (check(TokenType::KwSelf)) { p.name = "self"; advance(); }
                else if (check(TokenType::Amp) && peek(1).type == TokenType::KwSelf) {
                    p.name = "self"; advance(); advance();
                }
                else if (check(TokenType::KwMut) && peek(1).type == TokenType::KwSelf) {
                    p.name = "self"; advance(); advance();
                }
                else if (check(TokenType::Amp) && peek(1).type == TokenType::KwMut &&
                    peek(2).type == TokenType::KwSelf) {
                    p.name = "self"; advance(); advance(); advance();
                }
                else {
                    p.name = expect(TokenType::Identifier, "param name").lexeme;
                }
                if (match(TokenType::Colon)) p.type = parseType();
                fn->params.push_back(std::move(p));
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RParen, "')'");

        if (match(TokenType::Arrow)) fn->returnType = parseType();

        if (check(TokenType::LBrace)) {
            fn->body = parseBlock();
        }
        else {
            if (match(TokenType::Assign)) {
                auto block = std::make_unique<BlockStmt>();
                auto stmt = std::make_unique<ReturnStmt>();
                stmt->value = parseExpression();
                block->statements.push_back(std::move(stmt));
                fn->body = std::move(block);
            }
            match(TokenType::Semi);
        }
        return fn;
    }

    DeclPtr Parser::parseStructDecl(bool isPub) {
        (void)isPub;
        auto st = std::make_unique<StructDecl>();
        st->line = peek().line;
        expect(TokenType::KwStruct, "'struct'");
        st->name = expect(TokenType::Identifier, "struct name").lexeme;
        expect(TokenType::LBrace, "'{'");
        while (!check(TokenType::RBrace) && !isAtEnd()) {
            StructField f;
            f.name = expect(TokenType::Identifier, "field name").lexeme;
            expect(TokenType::Colon, "':'");
            f.type = parseType();
            match(TokenType::Comma);
            st->fields.push_back(std::move(f));
        }
        expect(TokenType::RBrace, "'}'");
        return st;
    }

    DeclPtr Parser::parseEnumDecl(bool isPub) {
        (void)isPub;
        auto en = std::make_unique<EnumDecl>();
        en->line = peek().line;
        expect(TokenType::KwEnum, "'enum'");
        en->name = expect(TokenType::Identifier, "enum name").lexeme;
        expect(TokenType::LBrace, "'{'");
        while (!check(TokenType::RBrace) && !isAtEnd()) {
            EnumVariant v;
            v.name = expect(TokenType::Identifier, "variant name").lexeme;
            if (match(TokenType::LParen)) {
                if (!check(TokenType::RParen)) {
                    do { v.payload.push_back(parseType()); } while (match(TokenType::Comma));
                }
                expect(TokenType::RParen, "')'");
            }
            match(TokenType::Comma);
            en->variants.push_back(std::move(v));
        }
        expect(TokenType::RBrace, "'}'");
        return en;
    }

    DeclPtr Parser::parseImplDecl() {
        auto impl = std::make_unique<ImplDecl>();
        impl->line = peek().line;
        expect(TokenType::KwImpl, "'impl'");
        impl->typeName = expect(TokenType::Identifier, "type name").lexeme;
        expect(TokenType::LBrace, "'{'");
        while (!check(TokenType::RBrace) && !isAtEnd()) {
            if (check(TokenType::KwFn)) {
                auto fn = parseFunctionDecl(false);
                if (fn) {
                    impl->methods.push_back(
                        std::unique_ptr<FunctionDecl>(
                            static_cast<FunctionDecl*>(fn.release())));
                }
            }
            else {
                error("Expected 'fn' inside impl");
                synchronize();
            }
        }
        expect(TokenType::RBrace, "'}'");
        return impl;
    }

    DeclPtr Parser::parseUseDecl() {
        auto use = std::make_unique<UseDecl>();
        use->line = peek().line;
        expect(TokenType::KwUse, "'use'");
        do { use->path.push_back(expect(TokenType::Identifier, "path segment").lexeme); } while (match(TokenType::ColonColon));
        match(TokenType::Semi);
        return use;
    }

    // ─── Statements ────────────────────────────────────────
    StmtPtr Parser::parseStatement() {
        if (check(TokenType::KwLet))      return parseLetStatement();
        if (check(TokenType::KwIf))       return parseIfStatement();
        if (check(TokenType::Question))   return parseIfStatement();  // ★ ? = if
        if (check(TokenType::KwWhile))    return parseWhileStatement();
        if (check(TokenType::KwFor))      return parseForStatement();
        if (check(TokenType::KwLoop))     return parseLoopStatement();
        if (check(TokenType::KwReturn))   return parseReturnStatement();
        if (check(TokenType::KwBreak))    return parseBreakStatement();
        if (check(TokenType::KwContinue)) return parseContinueStatement();
        if (check(TokenType::LBrace))     return parseBlock();

        auto stmt = std::make_unique<ExprStmt>();
        stmt->line = peek().line;
        stmt->expr = parseExpression();
        match(TokenType::Semi);
        return stmt;
    }

    StmtPtr Parser::parseLetStatement() {
        auto stmt = std::make_unique<LetStmt>();
        stmt->line = peek().line;
        expect(TokenType::KwLet, "'let'");
        stmt->isMut = match(TokenType::KwMut);
        stmt->name = expect(TokenType::Identifier, "variable name").lexeme;
        if (match(TokenType::Colon)) stmt->declaredType = parseType();
        if (match(TokenType::Assign)) stmt->init = parseExpression();
        expect(TokenType::Semi, "';'");
        return stmt;
    }

    StmtPtr Parser::parseIfStatement() {
        auto stmt = std::make_unique<IfStmt>();
        stmt->line = peek().line;

        // ★ if или ?
        bool isQuestion = false;
        if (match(TokenType::KwIf)) {
            isQuestion = false;
        }
        else {
            expect(TokenType::Question, "'if' or '?'");
            isQuestion = true;
        }

        stmt->condition = parseExpression();

        if (check(TokenType::LBrace)) {
            stmt->thenBranch = parseBlock();
        }
        else {
            error("Expected '{' after if condition");
            synchronize();
        }

        // ★ else или : (для ? синтаксиса)
        if (match(TokenType::KwElse)) {
            if (check(TokenType::KwIf)) {
                stmt->elseBranch = parseIfStatement();
            }
            else if (check(TokenType::LBrace)) {
                stmt->elseBranch = parseBlock();
            }
            else {
                error("Expected 'if' or '{' after else");
            }
        }
        else if (isQuestion && match(TokenType::Colon)) {
            // ★ : как else (только для ?)
            if (check(TokenType::KwIf) || check(TokenType::Question)) {
                stmt->elseBranch = parseIfStatement();
            }
            else if (check(TokenType::LBrace)) {
                stmt->elseBranch = parseBlock();
            }
            else {
                error("Expected 'if' or '{' after :");
            }
        }

        return stmt;
    }

    StmtPtr Parser::parseWhileStatement() {
        auto stmt = std::make_unique<WhileStmt>();
        stmt->line = peek().line;
        expect(TokenType::KwWhile, "'while'");
        stmt->condition = parseExpression();
        stmt->body = parseBlock();
        return stmt;
    }

    StmtPtr Parser::parseForStatement() {
        auto stmt = std::make_unique<ForStmt>();
        stmt->line = peek().line;
        expect(TokenType::KwFor, "'for'");
        stmt->varName = expect(TokenType::Identifier, "loop variable").lexeme;
        expect(TokenType::KwIn, "'in'");
        auto first = parseAssignment();
        if (match(TokenType::DotDot)) {
            auto second = parseAssignment();
            auto range = std::make_unique<BinaryExpr>();
            range->line = stmt->line;
            range->op = "..";
            range->left = std::move(first);
            range->right = std::move(second);
            stmt->iterable = std::move(range);
        }
        else {
            stmt->iterable = std::move(first);
        }
        stmt->body = parseBlock();
        return stmt;
    }

    StmtPtr Parser::parseLoopStatement() {
        auto stmt = std::make_unique<LoopStmt>();
        stmt->line = peek().line;
        expect(TokenType::KwLoop, "'loop'");
        stmt->body = parseBlock();
        return stmt;
    }

    StmtPtr Parser::parseReturnStatement() {
        auto stmt = std::make_unique<ReturnStmt>();
        stmt->line = peek().line;
        expect(TokenType::KwReturn, "'return'");
        if (!check(TokenType::Semi)) stmt->value = parseExpression();
        expect(TokenType::Semi, "';'");
        return stmt;
    }

    StmtPtr Parser::parseBreakStatement() {
        auto stmt = std::make_unique<BreakStmt>();
        expect(TokenType::KwBreak, "'break'");
        expect(TokenType::Semi, "';'");
        return stmt;
    }

    StmtPtr Parser::parseContinueStatement() {
        auto stmt = std::make_unique<ContinueStmt>();
        expect(TokenType::KwContinue, "'continue'");
        expect(TokenType::Semi, "';'");
        return stmt;
    }

    std::unique_ptr<BlockStmt> Parser::parseBlock() {
        auto block = std::make_unique<BlockStmt>();
        block->line = peek().line;
        expect(TokenType::LBrace, "'{'");
        while (!check(TokenType::RBrace) && !isAtEnd()) {
            auto stmt = parseStatement();
            if (stmt) block->statements.push_back(std::move(stmt));
        }
        expect(TokenType::RBrace, "'}'");
        return block;
    }

    // ─── Expressions ───────────────────────────────────────
    ExprPtr Parser::parseExpression() { return parseAssignment(); }

    ExprPtr Parser::parseAssignment() {
        auto left = parseOr();
        if (matchAny({ TokenType::Assign, TokenType::PlusEq, TokenType::MinusEq,
                       TokenType::StarEq, TokenType::SlashEq, TokenType::PercentEq })) {
            auto assign = std::make_unique<AssignExpr>();
            assign->line = previous().line;
            assign->op = previous().lexeme;
            assign->target = std::move(left);
            assign->value = parseAssignment();
            return assign;
        }
        return left;
    }

    ExprPtr Parser::parseOr() {
        auto left = parseAnd();
        while (match(TokenType::Or)) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = "||";
            bin->left = std::move(left);
            bin->right = parseAnd();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseAnd() {
        auto left = parseEquality();
        while (match(TokenType::And)) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = "&&";
            bin->left = std::move(left);
            bin->right = parseEquality();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseEquality() {
        auto left = parseComparison();
        while (matchAny({ TokenType::Eq, TokenType::NotEq })) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = previous().lexeme;
            bin->left = std::move(left);
            bin->right = parseComparison();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseComparison() {
        auto left = parseAddition();
        while (matchAny({ TokenType::Lt, TokenType::Gt, TokenType::LtEq, TokenType::GtEq })) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = previous().lexeme;
            bin->left = std::move(left);
            bin->right = parseAddition();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseAddition() {
        auto left = parseMultiplication();
        while (matchAny({ TokenType::Plus, TokenType::Minus })) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = previous().lexeme;
            bin->left = std::move(left);
            bin->right = parseMultiplication();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseMultiplication() {
        auto left = parseUnary();
        while (matchAny({ TokenType::Star, TokenType::Slash, TokenType::Percent })) {
            auto bin = std::make_unique<BinaryExpr>();
            bin->line = previous().line;
            bin->op = previous().lexeme;
            bin->left = std::move(left);
            bin->right = parseUnary();
            left = std::move(bin);
        }
        return left;
    }

    ExprPtr Parser::parseUnary() {
        if (matchAny({ TokenType::Not, TokenType::Minus, TokenType::Tilde })) {
            auto un = std::make_unique<UnaryExpr>();
            un->line = previous().line;
            un->op = previous().lexeme;
            un->operand = parseUnary();
            return un;
        }
        return parsePostfix();
    }

    ExprPtr Parser::parsePostfix() {
        auto expr = parsePrimary();
        while (true) {
            if (match(TokenType::LParen)) {
                auto call = std::make_unique<CallExpr>();
                call->line = previous().line;
                call->callee = std::move(expr);
                if (!check(TokenType::RParen)) {
                    do { call->args.push_back(parseExpression()); } while (match(TokenType::Comma));
                }
                expect(TokenType::RParen, "')'");
                expr = std::move(call);
                continue;
            }
            if (match(TokenType::Dot)) {
                auto field = std::make_unique<FieldExpr>();
                field->line = previous().line;
                field->object = std::move(expr);
                field->field = expect(TokenType::Identifier, "field name").lexeme;
                expr = std::move(field);
                continue;
            }
            if (match(TokenType::LBracket)) {
                auto index = std::make_unique<IndexExpr>();
                index->line = previous().line;
                index->object = std::move(expr);
                index->index = parseExpression();
                expect(TokenType::RBracket, "']'");
                expr = std::move(index);
                continue;
            }
            break;
        }
        return expr;
    }

    ExprPtr Parser::parsePrimary() {
        if (match(TokenType::Integer)) {
            auto lit = std::make_unique<IntLitExpr>();
            lit->line = previous().line;
            try { lit->value = std::stoll(previous().lexeme); }
            catch (...) { lit->value = 0; }
            return lit;
        }
        if (match(TokenType::Float)) {
            auto lit = std::make_unique<FloatLitExpr>();
            lit->line = previous().line;
            try { lit->value = std::stod(previous().lexeme); }
            catch (...) { lit->value = 0.0; }
            return lit;
        }
        if (match(TokenType::String)) {
            auto lit = std::make_unique<StringLitExpr>();
            lit->line = previous().line;
            lit->value = previous().lexeme;
            return lit;
        }
        if (match(TokenType::Char)) {
            auto lit = std::make_unique<CharLitExpr>();
            lit->line = previous().line;
            lit->value = previous().lexeme.empty() ? '\0' : previous().lexeme[0];
            return lit;
        }
        if (match(TokenType::True)) {
            auto lit = std::make_unique<BoolLitExpr>();
            lit->line = previous().line; lit->value = true; return lit;
        }
        if (match(TokenType::False)) {
            auto lit = std::make_unique<BoolLitExpr>();
            lit->line = previous().line; lit->value = false; return lit;
        }
        if (match(TokenType::LParen)) {
            auto group = std::make_unique<GroupExpr>();
            group->line = previous().line;
            group->inner = parseExpression();
            expect(TokenType::RParen, "')'");
            return group;
        }
        if (match(TokenType::LBracket)) {
            auto arr = std::make_unique<ArrayLitExpr>();
            arr->line = previous().line;
            if (!check(TokenType::RBracket)) {
                do { arr->elements.push_back(parseExpression()); } while (match(TokenType::Comma));
            }
            expect(TokenType::RBracket, "']'");
            return arr;
        }
        if (match(TokenType::KwSelf)) {
            auto id = std::make_unique<IdentifierExpr>();
            id->line = previous().line; id->name = "self"; return id;
        }
        if (check(TokenType::Identifier)) {
            std::string name = peek().lexeme;
            bool isStructLit = false;
            if (peek(1).type == TokenType::LBrace) {
                if (peek(2).type == TokenType::Identifier && peek(3).type == TokenType::Colon)
                    isStructLit = true;
                else if (peek(2).type == TokenType::RBrace)
                    isStructLit = true;
            }
            if (isStructLit) {
                advance(); advance();
                auto lit = std::make_unique<StructLitExpr>();
                lit->line = previous().line; lit->typeName = name;
                while (!check(TokenType::RBrace) && !isAtEnd()) {
                    StructLitField field;
                    field.name = expect(TokenType::Identifier, "field name").lexeme;
                    expect(TokenType::Colon, "':'");
                    field.value = parseExpression();
                    lit->fields.push_back(std::move(field));
                    if (!match(TokenType::Comma)) break;
                }
                expect(TokenType::RBrace, "'}'");
                return lit;
            }
            advance();
            if (check(TokenType::ColonColon)) {
                auto path = std::make_unique<PathExpr>();
                path->line = previous().line;
                path->segments.push_back(name);
                while (match(TokenType::ColonColon)) {
                    path->segments.push_back(
                        expect(TokenType::Identifier, "path segment").lexeme);
                }
                return path;
            }
            auto id = std::make_unique<IdentifierExpr>();
            id->line = previous().line; id->name = name; return id;
        }
        error("Expected expression, got '" + peek().lexeme + "'");
        advance();
        return nullptr;
    }

} // namespace mayami