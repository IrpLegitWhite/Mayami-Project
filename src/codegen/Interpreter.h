// src/codegen/Interpreter.h
#pragma once

#include "Value.h"
#include "Environment.h"
#include "../parser/AST.h"
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>

namespace mayami {

    using OutputCallback = std::function<void(const std::string&)>;

    struct ReturnException {
        ValuePtr value;
    };
    struct BreakException {};
    struct ContinueException {};

    class Interpreter {
    public:
        Interpreter();

        void setOutputCallback(OutputCallback cb) { m_output = std::move(cb); }
        void setErrorCallback(OutputCallback cb) { m_error = std::move(cb); }

        bool run(Program* program);

        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

    private:
        // ─── Decl ───────────────────────────────────────────
        void visitFunctionDecl(FunctionDecl* decl);
        void visitStructDecl(StructDecl* decl);
        void visitEnumDecl(EnumDecl* decl);
        void visitImplDecl(ImplDecl* decl);

        // ─── Stmt ───────────────────────────────────────────
        void visitStmt(Stmt* stmt);
        void visitLetStmt(LetStmt* stmt);
        void visitExprStmt(ExprStmt* stmt);
        void visitBlockStmt(BlockStmt* stmt);
        void visitIfStmt(IfStmt* stmt);
        void visitWhileStmt(WhileStmt* stmt);
        void visitForStmt(ForStmt* stmt);
        void visitLoopStmt(LoopStmt* stmt);
        void visitReturnStmt(ReturnStmt* stmt);
        void visitBreakStmt(BreakStmt* stmt);
        void visitContinueStmt(ContinueStmt* stmt);

        // ─── Expr ───────────────────────────────────────────
        ValuePtr visitExpr(Expr* expr);
        ValuePtr visitIntLit(IntLitExpr* expr);
        ValuePtr visitFloatLit(FloatLitExpr* expr);
        ValuePtr visitStringLit(StringLitExpr* expr);
        ValuePtr visitCharLit(CharLitExpr* expr);
        ValuePtr visitBoolLit(BoolLitExpr* expr);
        ValuePtr visitIdentifier(IdentifierExpr* expr);
        ValuePtr visitUnary(UnaryExpr* expr);
        ValuePtr visitBinary(BinaryExpr* expr);
        ValuePtr visitAssign(AssignExpr* expr);
        ValuePtr visitCall(CallExpr* expr);
        ValuePtr visitField(FieldExpr* expr);
        ValuePtr visitIndex(IndexExpr* expr);
        ValuePtr visitGroup(GroupExpr* expr);
        ValuePtr visitArrayLit(ArrayLitExpr* expr);
        ValuePtr visitStructLit(StructLitExpr* expr);

        // ─── Функции ────────────────────────────────────────
        ValuePtr callFunction(FunctionDecl* fn,
            const std::vector<ValuePtr>& args);
        ValuePtr callBuiltin(const std::string& name,
            const std::vector<ValuePtr>& args);
        ValuePtr callStringMethod(ValuePtr obj,
            const std::string& method,
            const std::vector<ValuePtr>& args);

        void error(const std::string& msg);

        // ─── State ──────────────────────────────────────────
        std::shared_ptr<Environment> m_globals;
        std::shared_ptr<Environment> m_env;

        std::unordered_map<std::string, FunctionDecl*> m_functions;

        struct StructDef {
            std::string name;
            std::vector<StructField> fields;
        };
        std::unordered_map<std::string, StructDef> m_structs;

        std::unordered_map<std::string,
            std::unordered_map<std::string, FunctionDecl*>> m_methods;

        OutputCallback m_output;
        OutputCallback m_error;

        std::vector<std::string> m_errors;

        int m_callDepth = 0;
        static constexpr int MAX_CALL_DEPTH = 1000;
    };

} // namespace mayami