// src/codegen/BytecodeGen.h
#pragma once

#include "../parser/AST.h"
#include "../vm/Bytecode.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>

namespace mayami {

    // ============================================================
    // BytecodeGen — генератор байткода
    // ============================================================
    class BytecodeGen {
    public:
        BytecodeGen();

        // Генерация байткода из программы
        std::vector<uint8_t> generate(Program* program);

        // Ошибки
        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

        // ★ Папка исходного файла — для поиска import
        void setSourceDir(const std::string& dir) { m_sourceDir = dir; }

    private:
        // ─── Информация о функции ───────────────────────────
        struct FuncInfo {
            std::string name;
            uint32_t nameOffset = 0;
            uint32_t codeOffset = 0;
            uint32_t localCount = 0;
            uint8_t paramCount = 0;
            std::vector<std::string> paramNames;
        };

        // ─── Loop context (для break/continue) ──────────────
        struct LoopContext {
            std::vector<size_t> breakJumps;
            std::vector<size_t> continueJumps;
            size_t continueTarget = 0;
        };

        // ─── Эмиссия байткода ───────────────────────────────
        void emit(OpCode op);
        void emitU8(uint8_t v);
        void emitU32(uint32_t v);
        void emitI64(int64_t v);
        void emitF64(double v);
        size_t emitJump(OpCode op);
        void patchJump(size_t pos);

        // ─── Пул строк ──────────────────────────────────────
        uint32_t internString(const std::string& s);

        // ─── Переменные ─────────────────────────────────────
        uint32_t declareVar(const std::string& name);
        int findVar(const std::string& name);

        // ─── Функции ────────────────────────────────────────
        int findFunc(const std::string& name);
        void collectFunctions(Program* program);
        void genFunction(FunctionDecl* fn);
        void genMainBody(BlockStmt* body);

        // ─── Statements ─────────────────────────────────────
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

        // ─── Expressions ────────────────────────────────────
        void genExpr(Expr* expr);
        void genIntLit(IntLitExpr* e);
        void genFloatLit(FloatLitExpr* e);
        void genStringLit(StringLitExpr* e);
        void genCharLit(CharLitExpr* e);
        void genBoolLit(BoolLitExpr* e);
        void genIdentifier(IdentifierExpr* e);
        void genUnary(UnaryExpr* e);
        void genBinary(BinaryExpr* e);
        void genAssign(AssignExpr* e);
        void genCall(CallExpr* e);
        void genField(FieldExpr* e);
        void genIndex(IndexExpr* e);
        void genGroup(GroupExpr* e);
        void genArrayLit(ArrayLitExpr* e);
        void genStructLit(StructLitExpr* e);
        void genPath(PathExpr* e);

        // ─── Import ★ ───────────────────────────────────────
        void loadImport(const std::string& path, Program* program);

        // ─── Ошибки ─────────────────────────────────────────
        void error(const std::string& msg);

        // ─── Состояние ──────────────────────────────────────
        std::vector<uint8_t> m_code;                        // байткод
        std::vector<uint8_t> m_constants;                   // пул констант
        std::unordered_map<std::string, uint32_t> m_stringPool;  // строки → offset

        std::vector<FuncInfo> m_funcs;                      // все функции
        std::unordered_map<std::string, uint32_t> m_funcByName;

        std::unordered_map<std::string, uint32_t> m_vars;   // переменные → слоты
        uint32_t m_varCount = 0;                            // счётчик слотов

        std::vector<LoopContext> m_loops;                   // стек циклов

        uint32_t m_entryPoint = 0;
        uint32_t m_currentFuncIdx = 0;
        bool m_inFunction = false;

        std::vector<std::string> m_errors;

        // ★ Для import
        std::string m_sourceDir;                            // папка исходника
        std::set<std::string> m_importedFiles;              // защита от дублей/циклов
    };

} // namespace mayami