// src/codegen/BytecodeGen.cpp
#include "BytecodeGen.h"
#include <cstring>
#include <cstdio>
#include <sstream>

namespace mayami {

    BytecodeGen::BytecodeGen() {}

    void BytecodeGen::error(const std::string& msg) {
        m_errors.push_back(msg);
    }

    // ============================================================
    // Эмиссия
    // ============================================================
    void BytecodeGen::emit(OpCode op) {
        m_code.push_back((uint8_t)op);
    }

    void BytecodeGen::emitU8(uint8_t v) {
        m_code.push_back(v);
    }

    void BytecodeGen::emitU32(uint32_t v) {
        mayami::writeU32(m_code, v);
    }

    void BytecodeGen::emitI64(int64_t v) {
        mayami::writeI64(m_code, v);
    }

    void BytecodeGen::emitF64(double v) {
        mayami::writeF64(m_code, v);
    }

    size_t BytecodeGen::emitJump(OpCode op) {
        emit(op);
        size_t pos = m_code.size();
        emitU32(0);
        return pos;
    }

    void BytecodeGen::patchJump(size_t pos) {
        uint32_t target = (uint32_t)m_code.size();
        m_code[pos + 0] = (uint8_t)(target & 0xFF);
        m_code[pos + 1] = (uint8_t)((target >> 8) & 0xFF);
        m_code[pos + 2] = (uint8_t)((target >> 16) & 0xFF);
        m_code[pos + 3] = (uint8_t)((target >> 24) & 0xFF);
    }

    // ============================================================
    // Пул строк
    // ============================================================
    uint32_t BytecodeGen::internString(const std::string& s) {
        auto it = m_stringPool.find(s);
        if (it != m_stringPool.end()) return it->second;

        uint32_t offset = (uint32_t)m_constants.size();
        mayami::writeU32(m_constants, (uint32_t)s.size());
        m_constants.insert(m_constants.end(), s.begin(), s.end());

        m_stringPool[s] = offset;
        return offset;
    }

    // ============================================================
    // Переменные
    // ============================================================
    uint32_t BytecodeGen::declareVar(const std::string& name) {
        auto it = m_vars.find(name);
        if (it != m_vars.end()) return it->second;
        uint32_t slot = m_varCount++;
        m_vars[name] = slot;
        return slot;
    }

    int BytecodeGen::findVar(const std::string& name) {
        auto it = m_vars.find(name);
        if (it == m_vars.end()) return -1;
        return (int)it->second;
    }

    // ============================================================
    // Функции
    // ============================================================
    int BytecodeGen::findFunc(const std::string& name) {
        auto it = m_funcByName.find(name);
        if (it == m_funcByName.end()) return -1;
        return (int)it->second;
    }

    void BytecodeGen::collectFunctions(Program* program) {
        for (auto& decl : program->declarations) {
            if (decl->kind != DeclKind::Function) continue;
            auto fn = static_cast<FunctionDecl*>(decl.get());

            FuncInfo info;
            info.name = fn->name;
            info.nameOffset = internString(fn->name);
            info.paramCount = (uint8_t)fn->params.size();

            for (auto& p : fn->params)
                info.paramNames.push_back(p.name);

            uint32_t idx = (uint32_t)m_funcs.size();
            m_funcs.push_back(info);
            m_funcByName[fn->name] = idx;
        }
    }

    void BytecodeGen::genFunction(FunctionDecl* fn) {
        int idx = findFunc(fn->name);
        if (idx < 0) {
            error("Internal: function not collected: " + fn->name);
            return;
        }

        m_vars.clear();
        m_varCount = 0;
        m_loops.clear();

        m_inFunction = true;
        m_currentFuncIdx = (uint32_t)idx;

        m_funcs[idx].codeOffset = (uint32_t)m_code.size();

        for (auto& p : fn->params) {
            declareVar(p.name);
        }

        if (fn->body) {
            for (auto& stmt : fn->body->statements) {
                genStmt(stmt.get());
            }
        }

        emit(OpCode::PUSH_NULL);
        emit(OpCode::RET);

        m_funcs[idx].localCount = m_varCount;

        m_inFunction = false;
    }

    void BytecodeGen::genMainBody(BlockStmt* body) {
        m_vars.clear();
        m_varCount = 0;
        m_loops.clear();
        m_inFunction = false;

        for (auto& stmt : body->statements) {
            genStmt(stmt.get());
        }
    }

    // ============================================================
    // Statements
    // ============================================================
    void BytecodeGen::genStmt(Stmt* stmt) {
        if (!stmt) return;
        switch (stmt->kind) {
        case StmtKind::Let:      genLetStmt(static_cast<LetStmt*>(stmt)); break;
        case StmtKind::ExprStmt: genExprStmt(static_cast<ExprStmt*>(stmt)); break;
        case StmtKind::Block:    genBlockStmt(static_cast<BlockStmt*>(stmt)); break;
        case StmtKind::If:       genIfStmt(static_cast<IfStmt*>(stmt)); break;
        case StmtKind::While:    genWhileStmt(static_cast<WhileStmt*>(stmt)); break;
        case StmtKind::For:      genForStmt(static_cast<ForStmt*>(stmt)); break;
        case StmtKind::Loop:     genLoopStmt(static_cast<LoopStmt*>(stmt)); break;
        case StmtKind::Return:   genReturnStmt(static_cast<ReturnStmt*>(stmt)); break;
        case StmtKind::Break:    genBreakStmt(static_cast<BreakStmt*>(stmt)); break;
        case StmtKind::Continue: genContinueStmt(static_cast<ContinueStmt*>(stmt)); break;
        }
    }

    void BytecodeGen::genLetStmt(LetStmt* stmt) {
        if (stmt->init) {
            genExpr(stmt->init.get());
        }
        else {
            emit(OpCode::PUSH_NULL);
        }
        uint32_t slot = declareVar(stmt->name);
        emit(OpCode::STORE_VAR);
        emitU32(slot);
    }

    void BytecodeGen::genExprStmt(ExprStmt* stmt) {
        if (!stmt->expr) return;
        genExpr(stmt->expr.get());
        emit(OpCode::POP);
    }

    void BytecodeGen::genBlockStmt(BlockStmt* stmt) {
        for (auto& s : stmt->statements) {
            genStmt(s.get());
        }
    }

    void BytecodeGen::genIfStmt(IfStmt* stmt) {
        genExpr(stmt->condition.get());
        size_t elseJump = emitJump(OpCode::JUMP_IF_FALSE);

        genStmt(stmt->thenBranch.get());

        if (stmt->elseBranch) {
            size_t endJump = emitJump(OpCode::JUMP);
            patchJump(elseJump);
            genStmt(stmt->elseBranch.get());
            patchJump(endJump);
        }
        else {
            patchJump(elseJump);
        }
    }

    void BytecodeGen::genWhileStmt(WhileStmt* stmt) {
        size_t start = m_code.size();

        m_loops.push_back(LoopContext{});
        m_loops.back().continueTarget = start;

        genExpr(stmt->condition.get());
        size_t endJump = emitJump(OpCode::JUMP_IF_FALSE);

        genStmt(stmt->body.get());

        emit(OpCode::JUMP);
        emitU32((uint32_t)start);

        patchJump(endJump);

        for (size_t p : m_loops.back().breakJumps) patchJump(p);
        for (size_t p : m_loops.back().continueJumps) {
            m_code[p + 0] = (uint8_t)(start & 0xFF);
            m_code[p + 1] = (uint8_t)((start >> 8) & 0xFF);
            m_code[p + 2] = (uint8_t)((start >> 16) & 0xFF);
            m_code[p + 3] = (uint8_t)((start >> 24) & 0xFF);
        }

        m_loops.pop_back();
    }

    void BytecodeGen::genForStmt(ForStmt* stmt) {
        auto bin = static_cast<BinaryExpr*>(stmt->iterable.get());
        if (!bin || bin->op != "..") {
            error("for loop currently only supports a..b range");
            return;
        }

        uint32_t slot = declareVar(stmt->varName);

        genExpr(bin->left.get());
        emit(OpCode::STORE_VAR);
        emitU32(slot);

        size_t start = m_code.size();

        m_loops.push_back(LoopContext{});

        emit(OpCode::LOAD_VAR);
        emitU32(slot);
        genExpr(bin->right.get());
        emit(OpCode::LT);

        size_t endJump = emitJump(OpCode::JUMP_IF_FALSE);

        genStmt(stmt->body.get());

        size_t incTarget = m_code.size();
        m_loops.back().continueTarget = incTarget;

        emit(OpCode::LOAD_VAR);
        emitU32(slot);
        emit(OpCode::PUSH_INT);
        emitI64(1);
        emit(OpCode::ADD);
        emit(OpCode::STORE_VAR);
        emitU32(slot);

        emit(OpCode::JUMP);
        emitU32((uint32_t)start);

        patchJump(endJump);

        for (size_t p : m_loops.back().breakJumps) patchJump(p);
        for (size_t p : m_loops.back().continueJumps) {
            m_code[p + 0] = (uint8_t)(incTarget & 0xFF);
            m_code[p + 1] = (uint8_t)((incTarget >> 8) & 0xFF);
            m_code[p + 2] = (uint8_t)((incTarget >> 16) & 0xFF);
            m_code[p + 3] = (uint8_t)((incTarget >> 24) & 0xFF);
        }

        m_loops.pop_back();
    }

    void BytecodeGen::genLoopStmt(LoopStmt* stmt) {
        size_t start = m_code.size();

        m_loops.push_back(LoopContext{});
        m_loops.back().continueTarget = start;

        genStmt(stmt->body.get());

        emit(OpCode::JUMP);
        emitU32((uint32_t)start);

        for (size_t p : m_loops.back().breakJumps) patchJump(p);
        for (size_t p : m_loops.back().continueJumps) {
            m_code[p + 0] = (uint8_t)(start & 0xFF);
            m_code[p + 1] = (uint8_t)((start >> 8) & 0xFF);
            m_code[p + 2] = (uint8_t)((start >> 16) & 0xFF);
            m_code[p + 3] = (uint8_t)((start >> 24) & 0xFF);
        }

        m_loops.pop_back();
    }

    void BytecodeGen::genReturnStmt(ReturnStmt* stmt) {
        if (stmt->value) {
            genExpr(stmt->value.get());
        }
        else {
            emit(OpCode::PUSH_NULL);
        }
        emit(OpCode::RET);
    }

    void BytecodeGen::genBreakStmt(BreakStmt* stmt) {
        (void)stmt;
        if (m_loops.empty()) {
            error("break outside loop");
            return;
        }
        size_t p = emitJump(OpCode::JUMP);
        m_loops.back().breakJumps.push_back(p);
    }

    void BytecodeGen::genContinueStmt(ContinueStmt* stmt) {
        (void)stmt;
        if (m_loops.empty()) {
            error("continue outside loop");
            return;
        }
        size_t p = emitJump(OpCode::JUMP);
        m_loops.back().continueJumps.push_back(p);
    }

    // ============================================================
    // Expressions
    // ============================================================
    void BytecodeGen::genExpr(Expr* expr) {
        if (!expr) { emit(OpCode::PUSH_NULL); return; }
        switch (expr->kind) {
        case ExprKind::IntLit:     genIntLit(static_cast<IntLitExpr*>(expr)); break;
        case ExprKind::FloatLit:   genFloatLit(static_cast<FloatLitExpr*>(expr)); break;
        case ExprKind::StringLit:  genStringLit(static_cast<StringLitExpr*>(expr)); break;
        case ExprKind::CharLit:    genCharLit(static_cast<CharLitExpr*>(expr)); break;
        case ExprKind::BoolLit:    genBoolLit(static_cast<BoolLitExpr*>(expr)); break;
        case ExprKind::Identifier: genIdentifier(static_cast<IdentifierExpr*>(expr)); break;
        case ExprKind::Unary:      genUnary(static_cast<UnaryExpr*>(expr)); break;
        case ExprKind::Binary:     genBinary(static_cast<BinaryExpr*>(expr)); break;
        case ExprKind::Assign:     genAssign(static_cast<AssignExpr*>(expr)); break;
        case ExprKind::Call:       genCall(static_cast<CallExpr*>(expr)); break;
        case ExprKind::Field:      genField(static_cast<FieldExpr*>(expr)); break;
        case ExprKind::Index:      genIndex(static_cast<IndexExpr*>(expr)); break;
        case ExprKind::Group:      genGroup(static_cast<GroupExpr*>(expr)); break;
        case ExprKind::ArrayLit:   genArrayLit(static_cast<ArrayLitExpr*>(expr)); break;
        case ExprKind::StructLit:  genStructLit(static_cast<StructLitExpr*>(expr)); break;
        case ExprKind::Path:       genPath(static_cast<PathExpr*>(expr)); break;
        default: emit(OpCode::PUSH_NULL); break;
        }
    }

    void BytecodeGen::genIntLit(IntLitExpr* e) {
        emit(OpCode::PUSH_INT);
        emitI64((int64_t)e->value);
    }

    void BytecodeGen::genFloatLit(FloatLitExpr* e) {
        emit(OpCode::PUSH_FLOAT);
        emitF64(e->value);
    }

    void BytecodeGen::genStringLit(StringLitExpr* e) {
        uint32_t offset = internString(e->value);
        emit(OpCode::PUSH_STR);
        emitU32(offset);
    }

    void BytecodeGen::genCharLit(CharLitExpr* e) {
        emit(OpCode::PUSH_CHAR);
        emitU8((uint8_t)e->value);
    }

    void BytecodeGen::genBoolLit(BoolLitExpr* e) {
        emit(e->value ? OpCode::PUSH_TRUE : OpCode::PUSH_FALSE);
    }

    void BytecodeGen::genIdentifier(IdentifierExpr* e) {
        int slot = findVar(e->name);
        if (slot < 0) {
            slot = (int)declareVar(e->name);
        }
        emit(OpCode::LOAD_VAR);
        emitU32((uint32_t)slot);
    }

    void BytecodeGen::genUnary(UnaryExpr* e) {
        genExpr(e->operand.get());
        if (e->op == "-") emit(OpCode::NEG);
        else if (e->op == "!") emit(OpCode::NOT);
        else if (e->op == "~") emit(OpCode::BIT_NOT);
    }

    void BytecodeGen::genBinary(BinaryExpr* e) {
        genExpr(e->left.get());
        genExpr(e->right.get());

        const std::string& op = e->op;
        if (op == "+")  emit(OpCode::ADD);
        else if (op == "-")  emit(OpCode::SUB);
        else if (op == "*")  emit(OpCode::MUL);
        else if (op == "/")  emit(OpCode::DIV);
        else if (op == "%")  emit(OpCode::MOD);
        else if (op == "==") emit(OpCode::EQ);
        else if (op == "!=") emit(OpCode::NEQ);
        else if (op == "<")  emit(OpCode::LT);
        else if (op == ">")  emit(OpCode::GT);
        else if (op == "<=") emit(OpCode::LE);
        else if (op == ">=") emit(OpCode::GE);
        else if (op == "&&") emit(OpCode::AND);
        else if (op == "||") emit(OpCode::OR);
        else if (op == "&")  emit(OpCode::BIT_AND);
        else if (op == "|")  emit(OpCode::BIT_OR);
        else if (op == "^")  emit(OpCode::BIT_XOR);
        else if (op == "<<") emit(OpCode::SHL);
        else if (op == ">>") emit(OpCode::SHR);
        else error("Unknown binary op: " + op);
    }

    void BytecodeGen::genAssign(AssignExpr* e) {
        if (e->target->kind != ExprKind::Identifier) {
            error("Assignment target must be identifier");
            return;
        }
        auto id = static_cast<IdentifierExpr*>(e->target.get());
        int slot = findVar(id->name);
        if (slot < 0) {
            slot = (int)declareVar(id->name);
        }

        if (e->op == "=") {
            genExpr(e->value.get());
            emit(OpCode::STORE_VAR);
            emitU32((uint32_t)slot);
        }
        else {
            emit(OpCode::LOAD_VAR);
            emitU32((uint32_t)slot);
            genExpr(e->value.get());

            std::string binOp = e->op.substr(0, 1);
            if (binOp == "+") emit(OpCode::ADD);
            else if (binOp == "-") emit(OpCode::SUB);
            else if (binOp == "*") emit(OpCode::MUL);
            else if (binOp == "/") emit(OpCode::DIV);
            else if (binOp == "%") emit(OpCode::MOD);
            else error("Unknown assign op: " + e->op);

            emit(OpCode::STORE_VAR);
            emitU32((uint32_t)slot);
        }
    }

    void BytecodeGen::genCall(CallExpr* e) {
        // obj.method(...)
        if (e->callee->kind == ExprKind::Field) {
            auto field = static_cast<FieldExpr*>(e->callee.get());

            genExpr(field->object.get());

            for (auto& arg : e->args) {
                genExpr(arg.get());
            }

            static const std::unordered_map<std::string, uint32_t> methodIds = {
                { "len",        NATIVE_STR_LEN },
                { "length",     NATIVE_STR_LEN },
                { "upper",      NATIVE_STR_UPPER },
                { "lower",      NATIVE_STR_LOWER },
                { "substr",     NATIVE_STR_SUBSTR },
                { "substring",  NATIVE_STR_SUBSTR },
                { "find",       NATIVE_STR_FIND },
                { "contains",   NATIVE_STR_CONTAINS },
                { "replace",    NATIVE_STR_REPLACE },
            };

            auto it = methodIds.find(field->field);
            if (it != methodIds.end()) {
                emit(OpCode::CALL_NATIVE);
                emitU32(it->second);
                emitU8((uint8_t)(e->args.size() + 1));
            }
            else {
                error("Unknown method: " + field->field);
                emit(OpCode::PUSH_NULL);
            }
            return;
        }

        if (e->callee->kind != ExprKind::Identifier) {
            error("Call target must be identifier or method");
            emit(OpCode::PUSH_NULL);
            return;
        }

        auto id = static_cast<IdentifierExpr*>(e->callee.get());

        // 1. Пользовательская функция
        int funcIdx = findFunc(id->name);
        if (funcIdx >= 0) {
            for (auto& arg : e->args) {
                genExpr(arg.get());
            }
            emit(OpCode::CALL);
            emitU32((uint32_t)funcIdx);
            emitU8((uint8_t)e->args.size());
            return;
        }

        // 2. Native
        for (auto& arg : e->args) {
            genExpr(arg.get());
        }

        static const std::unordered_map<std::string, uint32_t> nativeIds = {
            // Вывод
            { "p",         NATIVE_PRINTLN },
            { "println",   NATIVE_PRINTLN },
            { "w",         NATIVE_PRINT },
            { "print",     NATIVE_PRINT },
            { "e",         NATIVE_EPRINT },
            { "eprint",    NATIVE_EPRINT },

            // Математика
            { "s",     NATIVE_SQRT },
            { "sqrt",  NATIVE_SQRT },
            { "a",     NATIVE_ABS },
            { "abs",   NATIVE_ABS },
            { "si",    NATIVE_SIN },
            { "sin",   NATIVE_SIN },
            { "co",    NATIVE_COS },
            { "cos",   NATIVE_COS },
            { "ta",    NATIVE_TAN },
            { "tan",   NATIVE_TAN },
            { "pw",    NATIVE_POW },
            { "pow",   NATIVE_POW },
            { "fl",    NATIVE_FLOOR },
            { "floor", NATIVE_FLOOR },
            { "ce",    NATIVE_CEIL },
            { "ceil",  NATIVE_CEIL },
            { "ro",    NATIVE_ROUND },
            { "round", NATIVE_ROUND },
            { "mn",    NATIVE_MIN },
            { "min",   NATIVE_MIN },
            { "mx",    NATIVE_MAX },
            { "max",   NATIVE_MAX },

            // Старый UI (WinAPI-контролы)
            { "ui_begin",     NATIVE_UI_BEGIN },
            { "ui_label",     NATIVE_UI_LABEL },
            { "ui_button",    NATIVE_UI_BUTTON },
            { "ui_input",     NATIVE_UI_INPUT },
            { "ui_checkbox",  NATIVE_UI_CHECKBOX },
            { "ui_show",      NATIVE_UI_SHOW },
            { "ui_clicked",   NATIVE_UI_CLICKED },
            { "ui_get",       NATIVE_UI_GETVAL },
            { "ui_textbox",   NATIVE_UI_TEXTBOX },

            // Event loop
            { "ui_running",   NATIVE_UI_RUNNING },
            { "ui_poll",      NATIVE_UI_POLL },
            { "ui_sleep",     NATIVE_UI_SLEEP },
            { "ui_set_label", NATIVE_UI_SET_LABEL },
            { "ui_set_text",  NATIVE_UI_SET_TEXT },
            { "ui_remove",    NATIVE_UI_REMOVE },
            { "ui_refresh",   NATIVE_UI_REFRESH },

            // ★ Кастомный UI
            { "ui_theme",      NATIVE_UI_THEME },
            { "ui_rect",       NATIVE_UI_RECT },
            { "ui_rect_round", NATIVE_UI_RECT_ROUND },
            { "ui_text",       NATIVE_UI_TEXT },
            { "ui_text_size",  NATIVE_UI_TEXT_SIZE },
            { "ui_button_c",   NATIVE_UI_BUTTON_C },
            { "ui_clear",      NATIVE_UI_CLEAR },
            { "ui_width",      NATIVE_UI_WIDTH },
            { "ui_height",     NATIVE_UI_HEIGHT },
            { "ui_mouse_x",    NATIVE_UI_MOUSE_X },
            { "ui_mouse_y",    NATIVE_UI_MOUSE_Y },
        };

        auto it = nativeIds.find(id->name);
        if (it != nativeIds.end()) {
            emit(OpCode::CALL_NATIVE);
            emitU32(it->second);
            emitU8((uint8_t)e->args.size());
            return;
        }

        error("Unknown function: '" + id->name + "'");
        emit(OpCode::PUSH_NULL);
    }

    void BytecodeGen::genField(FieldExpr* e) {
        (void)e;
        error("Field access not yet supported");
        emit(OpCode::PUSH_NULL);
    }

    void BytecodeGen::genIndex(IndexExpr* e) {
        genExpr(e->object.get());
        genExpr(e->index.get());
        emit(OpCode::ARRAY_GET);
    }

    void BytecodeGen::genGroup(GroupExpr* e) {
        genExpr(e->inner.get());
    }

    void BytecodeGen::genArrayLit(ArrayLitExpr* e) {
        (void)e;
        error("Array literals not yet supported");
        emit(OpCode::PUSH_NULL);
    }

    void BytecodeGen::genStructLit(StructLitExpr* e) {
        (void)e;
        error("Struct literals not yet supported");
        emit(OpCode::PUSH_NULL);
    }

    void BytecodeGen::genPath(PathExpr* e) {
        (void)e;
        error("Path expressions not yet supported");
        emit(OpCode::PUSH_NULL);
    }

    // ============================================================
    // Generate
    // ============================================================
    std::vector<uint8_t> BytecodeGen::generate(Program* program) {
        if (!program) return {};

        collectFunctions(program);

        FunctionDecl* mainFn = nullptr;
        for (auto& decl : program->declarations) {
            if (decl->kind == DeclKind::Function) {
                auto fn = static_cast<FunctionDecl*>(decl.get());
                if (fn->name == "main") { mainFn = fn; break; }
            }
        }

        if (!mainFn) {
            error("No 'main' function found");
            return {};
        }

        m_entryPoint = 0;
        genMainBody(mainFn->body.get());
        emit(OpCode::HALT);

        for (auto& decl : program->declarations) {
            if (decl->kind != DeclKind::Function) continue;
            auto fn = static_cast<FunctionDecl*>(decl.get());
            if (fn->name == "main") continue;
            if (!fn->body) continue;

            genFunction(fn);
        }

        std::vector<uint8_t> result;

        BytecodeHeader hdr;
        hdr.magic = BYTECODE_MAGIC;
        hdr.version = BYTECODE_VERSION;
        hdr.reserved = 0;
        hdr.codeSize = (uint32_t)m_code.size();
        hdr.constSize = (uint32_t)m_constants.size();
        hdr.funcCount = (uint32_t)m_funcs.size();
        hdr.structCount = 0;
        hdr.entryPoint = m_entryPoint;

        result.insert(result.end(),
            (const uint8_t*)&hdr,
            (const uint8_t*)&hdr + sizeof(BytecodeHeader));

        result.insert(result.end(), m_constants.begin(), m_constants.end());

        for (auto& f : m_funcs) {
            BytecodeFunction bf;
            bf.nameOffset = f.nameOffset;
            bf.codeOffset = f.codeOffset;
            bf.localCount = f.localCount;
            bf.paramCount = f.paramCount;
            bf.reserved[0] = bf.reserved[1] = bf.reserved[2] = 0;

            result.insert(result.end(),
                (const uint8_t*)&bf,
                (const uint8_t*)&bf + sizeof(BytecodeFunction));
        }

        result.insert(result.end(), m_code.begin(), m_code.end());

        return result;
    }

} // namespace mayami