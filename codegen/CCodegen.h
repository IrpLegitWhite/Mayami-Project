// src/codegen/CCodegen.h
#pragma once

#include "../parser/AST.h"
#include <string>
#include <sstream>
#include <unordered_map>

namespace mayami {

    // ============================================================
    // CCodegen — генерация C++ кода из AST Mayami
    // ============================================================
    class CCodegen {
    public:
        CCodegen();

        // Сгенерировать C++ код для программы
        std::string generate(Program* program);

    private:
        // ─── Decl ───────────────────────────────────────────
        void genFunctionDecl(FunctionDecl* decl);
        void genStructDecl(StructDecl* decl);
        void genImplDecl(ImplDecl* decl);

        // ─── Stmt ───────────────────────────────────────────
        void genStmt(Stmt* stmt);
        void genLetStmt(LetStmt* stmt);
        void genExprStmt(ExprStmt* stmt);
        void genBlockStmt(BlockStmt* stmt);
        void genIfStmt(IfStmt* stmt);
        void genWhileStmt(WhileStmt* stmt);
        void genForStmt(ForStmt* stmt);
        void genLoopStmt(LoopStmt* stmt);
        void genReturnStmt(ReturnStmt* stmt);
        void genBreakStmt(BreakStmt* stmt);
        void genContinueStmt(ContinueStmt* stmt);

        // ─── Expr ───────────────────────────────────────────
        std::string genExpr(Expr* expr);
        std::string genIntLit(IntLitExpr* e);
        std::string genFloatLit(FloatLitExpr* e);
        std::string genStringLit(StringLitExpr* e);
        std::string genCharLit(CharLitExpr* e);
        std::string genBoolLit(BoolLitExpr* e);
        std::string genIdentifier(IdentifierExpr* e);
        std::string genUnary(UnaryExpr* e);
        std::string genBinary(BinaryExpr* e);
        std::string genAssign(AssignExpr* e);
        std::string genCall(CallExpr* e);
        std::string genField(FieldExpr* e);
        std::string genIndex(IndexExpr* e);
        std::string genGroup(GroupExpr* e);
        std::string genArrayLit(ArrayLitExpr* e);
        std::string genStructLit(StructLitExpr* e);

        // ─── Types ──────────────────────────────────────────
        std::string cppType(TypePtr type);

        // ─── Утилиты ────────────────────────────────────────
        void indent();
        void outdent();
        std::string indentStr() const;

        // ─── State ──────────────────────────────────────────
        std::ostringstream m_out;
        int m_indent = 0;

        // Отслеживаем используемые структуры и методы
        std::unordered_map<std::string, bool> m_usedStructs;
        std::unordered_map<std::string, bool> m_usedArrayMethods;

        // Уникальные имена для массивов (используем std::vector)
        std::unordered_map<std::string, std::string> m_vars;
    };

} // namespace mayami