// src/codegen/BytecodeGen.h
#pragma once

#include "../parser/AST.h"
#include "../vm/Bytecode.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace mayami {

    class BytecodeGen {
    public:
        BytecodeGen();
        std::vector<uint8_t> generate(Program* program);
        const std::vector<std::string>& errors() const { return m_errors; }
        bool hasErrors() const { return !m_errors.empty(); }

    private:
        void collectFunctions(Program* program);
        void genFunction(FunctionDecl* fn);
        void genMainBody(BlockStmt* body);
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
        void emit(OpCode op);
        void emitU8(uint8_t v);
        void emitU32(uint32_t v);
        void emitI64(int64_t v);
        void emitF64(double v);
        size_t emitJump(OpCode op);
        void patchJump(size_t pos);
        uint32_t internString(const std::string& s);
        uint32_t declareVar(const std::string& name);
        int findVar(const std::string& name);
        int findFunc(const std::string& name);

        struct LoopContext {
            std::vector<size_t> breakJumps;
            std::vector<size_t> continueJumps;
            size_t continueTarget = 0;
        };
        std::vector<LoopContext> m_loops;

        void error(const std::string& msg);

        std::vector<uint8_t> m_code;
        std::vector<uint8_t> m_constants;
        std::unordered_map<std::string, uint32_t> m_stringPool;
        std::unordered_map<std::string, uint32_t> m_vars;
        uint32_t m_varCount = 0;

        struct FuncInfo {
            std::string name;
            uint32_t    nameOffset = 0;
            uint32_t    codeOffset = 0;
            uint32_t    localCount = 0;
            uint8_t     paramCount = 0;
            std::vector<std::string> paramNames;
        };
        std::vector<FuncInfo> m_funcs;
        std::unordered_map<std::string, uint32_t> m_funcByName;

        uint32_t m_entryPoint = 0;
        std::vector<std::string> m_errors;
        bool     m_inFunction = false;
        uint32_t m_currentFuncIdx = 0;
    };

} // namespace mayami