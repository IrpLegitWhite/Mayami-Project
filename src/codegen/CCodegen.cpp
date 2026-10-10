// src/codegen/CCodegen.cpp
#include "CCodegen.h"
#include <cstdio>

namespace mayami {

    CCodegen::CCodegen() {}

    // ============================================================
    // Утилиты
    // ============================================================
    void CCodegen::indent() { ++m_indent; }
    void CCodegen::outdent() { if (m_indent > 0) --m_indent; }

    std::string CCodegen::indentStr() const {
        return std::string(m_indent * 4, ' ');
    }

    // ============================================================
    // Types
    // ============================================================
    std::string CCodegen::cppType(TypePtr type) {
        if (!type) return "auto";

        switch (type->kind) {
        case TypeKind::Void:   return "void";
        case TypeKind::Bool:   return "bool";
        case TypeKind::I8:     return "int8_t";
        case TypeKind::I16:    return "int16_t";
        case TypeKind::I32:    return "int32_t";
        case TypeKind::I64:    return "int64_t";
        case TypeKind::U8:     return "uint8_t";
        case TypeKind::U16:    return "uint16_t";
        case TypeKind::U32:    return "uint32_t";
        case TypeKind::U64:    return "uint64_t";
        case TypeKind::F32:    return "float";
        case TypeKind::F64:    return "double";
        case TypeKind::String: return "std::string";
        case TypeKind::Char:   return "char";
        case TypeKind::Named:  return type->name;
        case TypeKind::Ref:    return cppType(type->inner) + "&";
        case TypeKind::MutRef: return cppType(type->inner) + "&";
        case TypeKind::Ptr:    return cppType(type->inner) + "*";
        case TypeKind::Array:  return "std::vector<" + cppType(type->inner) + ">";
        case TypeKind::Slice:  return "std::vector<" + cppType(type->inner) + ">";
        default: return "auto";
        }
    }

    // ============================================================
    // Generate
    // ============================================================
    std::string CCodegen::generate(Program* program) {
        if (!program) return "";

        // ─── Заголовок ──────────────────────────────────────
        m_out << "// ============================================\n";
        m_out << "// Generated from Mayami source\n";
        m_out << "// Auto-generated. Do not edit.\n";
        m_out << "// ============================================\n\n";

        m_out << "#include <iostream>\n";
        m_out << "#include <string>\n";
        m_out << "#include <vector>\n";
        m_out << "#include <cstdint>\n";
        m_out << "#include <cmath>\n";
        m_out << "#include <algorithm>\n\n";

        // ─── Первый проход: структуры ───────────────────────
        for (auto& decl : program->declarations) {
            if (decl->kind == DeclKind::Struct) {
                genStructDecl(static_cast<StructDecl*>(decl.get()));
            }
        }

        // ─── Второй проход: функции ─────────────────────────
        for (auto& decl : program->declarations) {
            if (decl->kind == DeclKind::Function) {
                genFunctionDecl(static_cast<FunctionDecl*>(decl.get()));
                m_out << "\n";
            }
            else if (decl->kind == DeclKind::Impl) {
                genImplDecl(static_cast<ImplDecl*>(decl.get()));
                m_out << "\n";
            }
        }

        return m_out.str();
    }

    // ============================================================
    // Decl
    // ============================================================
    void CCodegen::genStructDecl(StructDecl* decl) {
        m_out << "struct " << decl->name << " {\n";
        indent();

        for (const auto& field : decl->fields) {
            m_out << indentStr()
                << cppType(field.type) << " " << field.name << ";\n";
        }

        // Дефолтный конструктор
        m_out << "\n";
        m_out << indentStr() << decl->name << "() = default;\n";

        // Конструктор со всеми полями
        m_out << indentStr() << decl->name << "(";
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            m_out << cppType(decl->fields[i].type) << " " << decl->fields[i].name;
            if (i + 1 < decl->fields.size()) m_out << ", ";
        }
        m_out << ") : ";
        for (size_t i = 0; i < decl->fields.size(); ++i) {
            m_out << decl->fields[i].name << "(" << decl->fields[i].name << ")";
            if (i + 1 < decl->fields.size()) m_out << ", ";
        }
        m_out << " {}\n";

        outdent();
        m_out << "};\n\n";
    }

    void CCodegen::genFunctionDecl(FunctionDecl* decl) {
        // Тип возврата
        std::string retType = decl->returnType
            ? cppType(decl->returnType)
            : "void";

        m_out << retType << " " << decl->name << "(";

        // Параметры
        for (size_t i = 0; i < decl->params.size(); ++i) {
            const auto& p = decl->params[i];
            std::string paramType = p.type ? cppType(p.type) : "auto";
            m_out << paramType << " " << p.name;
            if (i + 1 < decl->params.size()) m_out << ", ";
        }

        m_out << ") ";

        if (decl->body) {
            genBlockStmt(decl->body.get());
        }
        else {
            m_out << "{}\n";
        }
    }

    void CCodegen::genImplDecl(ImplDecl* decl) {
        // Методы — как свободные функции с именем Struct_method
        for (auto& method : decl->methods) {
            std::string retType = method->returnType
                ? cppType(method->returnType)
                : "void";

            m_out << retType << " " << decl->typeName << "_" << method->name << "(";

            for (size_t i = 0; i < method->params.size(); ++i) {
                const auto& p = method->params[i];
                std::string paramType = p.type ? cppType(p.type) : decl->typeName;
                m_out << paramType << "& " << p.name;
                if (i + 1 < method->params.size()) m_out << ", ";
            }

            m_out << ") ";

            if (method->body) {
                genBlockStmt(method->body.get());
            }
            else {
                m_out << "{}\n";
            }
            m_out << "\n";
        }
    }

    // ============================================================
    // Stmt
    // ============================================================
    void CCodegen::genStmt(Stmt* stmt) {
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

    void CCodegen::genLetStmt(LetStmt* stmt) {
        m_out << indentStr();

        // Тип
        std::string type = stmt->declaredType
            ? cppType(stmt->declaredType)
            : "auto";

        m_out << type << " " << stmt->name;

        if (stmt->init) {
            m_out << " = " << genExpr(stmt->init.get());
        }

        m_out << ";\n";
    }

    void CCodegen::genExprStmt(ExprStmt* stmt) {
        if (!stmt->expr) return;
        m_out << indentStr() << genExpr(stmt->expr.get()) << ";\n";
    }

    void CCodegen::genBlockStmt(BlockStmt* stmt) {
        m_out << "{\n";
        indent();

        for (auto& s : stmt->statements) {
            genStmt(s.get());
        }

        outdent();
        m_out << indentStr() << "}\n";
    }

    void CCodegen::genIfStmt(IfStmt* stmt) {
        m_out << indentStr() << "if (" << genExpr(stmt->condition.get()) << ") ";
        genStmt(stmt->thenBranch.get());

        if (stmt->elseBranch) {
            m_out << indentStr() << "else ";
            genStmt(stmt->elseBranch.get());
        }
    }

    void CCodegen::genWhileStmt(WhileStmt* stmt) {
        m_out << indentStr() << "while (" << genExpr(stmt->condition.get()) << ") ";
        genStmt(stmt->body.get());
    }

    void CCodegen::genForStmt(ForStmt* stmt) {
        // ★ for x in array
        if (stmt->iterable->kind != ExprKind::Binary) {
            m_out << indentStr() << "for (auto " << stmt->varName
                << " : " << genExpr(stmt->iterable.get()) << ") ";
            genStmt(stmt->body.get());
            return;
        }

        // for i in a..b
        auto bin = static_cast<BinaryExpr*>(stmt->iterable.get());
        if (bin->op == "..") {
            std::string start = genExpr(bin->left.get());
            std::string end = genExpr(bin->right.get());
            m_out << indentStr() << "for (int " << stmt->varName << " = "
                << start << "; " << stmt->varName << " < " << end
                << "; ++" << stmt->varName << ") ";
            genStmt(stmt->body.get());
            return;
        }

        // for i in N (0..N)
        m_out << indentStr() << "for (int " << stmt->varName << " = 0; "
            << stmt->varName << " < " << genExpr(stmt->iterable.get())
            << "; ++" << stmt->varName << ") ";
        genStmt(stmt->body.get());
    }

    void CCodegen::genLoopStmt(LoopStmt* stmt) {
        m_out << indentStr() << "while (true) ";
        genStmt(stmt->body.get());
    }

    void CCodegen::genReturnStmt(ReturnStmt* stmt) {
        m_out << indentStr() << "return";
        if (stmt->value) {
            m_out << " " << genExpr(stmt->value.get());
        }
        m_out << ";\n";
    }

    void CCodegen::genBreakStmt(BreakStmt* stmt) {
        (void)stmt;
        m_out << indentStr() << "break;\n";
    }

    void CCodegen::genContinueStmt(ContinueStmt* stmt) {
        (void)stmt;
        m_out << indentStr() << "continue;\n";
    }

    // ============================================================
    // Expr
    // ============================================================
    std::string CCodegen::genExpr(Expr* expr) {
        if (!expr) return "0";

        switch (expr->kind) {
        case ExprKind::IntLit:     return genIntLit(static_cast<IntLitExpr*>(expr));
        case ExprKind::FloatLit:   return genFloatLit(static_cast<FloatLitExpr*>(expr));
        case ExprKind::StringLit:  return genStringLit(static_cast<StringLitExpr*>(expr));
        case ExprKind::CharLit:    return genCharLit(static_cast<CharLitExpr*>(expr));
        case ExprKind::BoolLit:    return genBoolLit(static_cast<BoolLitExpr*>(expr));
        case ExprKind::Identifier: return genIdentifier(static_cast<IdentifierExpr*>(expr));
        case ExprKind::Unary:      return genUnary(static_cast<UnaryExpr*>(expr));
        case ExprKind::Binary:     return genBinary(static_cast<BinaryExpr*>(expr));
        case ExprKind::Assign:     return genAssign(static_cast<AssignExpr*>(expr));
        case ExprKind::Call:       return genCall(static_cast<CallExpr*>(expr));
        case ExprKind::Field:      return genField(static_cast<FieldExpr*>(expr));
        case ExprKind::Index:      return genIndex(static_cast<IndexExpr*>(expr));
        case ExprKind::Group:      return genGroup(static_cast<GroupExpr*>(expr));
        case ExprKind::ArrayLit:   return genArrayLit(static_cast<ArrayLitExpr*>(expr));
        case ExprKind::StructLit:  return genStructLit(static_cast<StructLitExpr*>(expr));
        default: return "0";
        }
    }

    std::string CCodegen::genIntLit(IntLitExpr* e) {
        return std::to_string(e->value);
    }

    std::string CCodegen::genFloatLit(FloatLitExpr* e) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", e->value);
        return buf;
    }

    std::string CCodegen::genStringLit(StringLitExpr* e) {
        // Экранируем кавычки и слеши
        std::string s = e->value;
        std::string r;
        for (char c : s) {
            if (c == '"')       r += "\\\"";
            else if (c == '\\') r += "\\\\";
            else if (c == '\n') r += "\\n";
            else if (c == '\t') r += "\\t";
            else if (c == '\r') r += "\\r";
            else r += c;
        }
        return "std::string(\"" + r + "\")";
    }

    std::string CCodegen::genCharLit(CharLitExpr* e) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "'%c'", e->value);
        return buf;
    }

    std::string CCodegen::genBoolLit(BoolLitExpr* e) {
        return e->value ? "true" : "false";
    }

    std::string CCodegen::genIdentifier(IdentifierExpr* e) {
        return e->name;
    }

    std::string CCodegen::genUnary(UnaryExpr* e) {
        return "(" + e->op + genExpr(e->operand.get()) + ")";
    }

    std::string CCodegen::genBinary(BinaryExpr* e) {
        // ★ Конкатенация строк — особый случай
        std::string left = genExpr(e->left.get());
        std::string right = genExpr(e->right.get());

        // Для строк используем std::string конкатенацию
        if (e->op == "+") {
            // Проверяем — есть ли строка в выражениях
            // (упрощённо: если хоть что-то — std::string)
            return "(" + left + " + " + right + ")";
        }

        return "(" + left + " " + e->op + " " + right + ")";
    }

    std::string CCodegen::genAssign(AssignExpr* e) {
        return genExpr(e->target.get()) + " " + e->op + " " + genExpr(e->value.get());
    }

    std::string CCodegen::genCall(CallExpr* e) {
        // ─── Методы ─────────────────────────────────────────
        if (e->callee->kind == ExprKind::Field) {
            auto field = static_cast<FieldExpr*>(e->callee.get());
            std::string obj = genExpr(field->object.get());
            std::string method = field->field;

            // Строковые методы
            if (method == "len" || method == "length") {
                return obj + ".size()";
            }
            if (method == "upper") {
                std::string tmp = obj;
                return "[](){ std::string _s = " + obj + "; for (auto& c : _s) c = std::toupper(c); return _s; }()";
            }
            if (method == "lower") {
                return "[](){ std::string _s = " + obj + "; for (auto& c : _s) c = std::tolower(c); return _s; }()";
            }

            // Метод структуры: Struct_method(obj, args...)
            std::string r = method + "(" + obj;
            for (auto& arg : e->args) {
                r += ", " + genExpr(arg.get());
            }
            r += ")";
            return r;
        }

        // ─── Обычная функция ────────────────────────────────
        std::string name;
        if (e->callee->kind == ExprKind::Identifier) {
            auto id = static_cast<IdentifierExpr*>(e->callee.get());
            name = id->name;
        }

        // ★ Алиасы → C++ функции
        static const std::unordered_map<std::string, std::string> cppAliases = {
            { "p", "println" }, { "w", "print" },
            { "s", "std::sqrt" }, { "a", "std::abs" },
            { "si", "std::sin" }, { "co", "std::cos" }, { "ta", "std::tan" },
            { "pw", "std::pow" }, { "fl", "std::floor" },
            { "ce", "std::ceil" }, { "ro", "std::round" },
            { "mn", "std::min" }, { "mx", "std::max" },
            { "sqrt", "std::sqrt" }, { "abs", "std::abs" },
            { "sin", "std::sin" }, { "cos", "std::cos" }, { "tan", "std::tan" },
            { "pow", "std::pow" },
            { "floor", "std::floor" }, { "ceil", "std::ceil" }, { "round", "std::round" },
            { "min", "std::min" }, { "max", "std::max" },
        };

        auto it = cppAliases.find(name);
        if (it != cppAliases.end()) {
            name = it->second;
        }

        std::string r = name + "(";
        for (size_t i = 0; i < e->args.size(); ++i) {
            r += genExpr(e->args[i].get());
            if (i + 1 < e->args.size()) r += ", ";
        }
        r += ")";
        return r;
    }

    std::string CCodegen::genField(FieldExpr* e) {
        return genExpr(e->object.get()) + "." + e->field;
    }

    std::string CCodegen::genIndex(IndexExpr* e) {
        return genExpr(e->object.get()) + "[" + genExpr(e->index.get()) + "]";
    }

    std::string CCodegen::genGroup(GroupExpr* e) {
        return "(" + genExpr(e->inner.get()) + ")";
    }

    std::string CCodegen::genArrayLit(ArrayLitExpr* e) {
        std::string r = "std::vector<";
        // Тип элементов пока не знаем — используем auto через decltype
        // Упрощённо: вектор с общим типом
        r += "decltype(";
        if (!e->elements.empty()) {
            r += genExpr(e->elements[0].get());
        }
        else {
            r += "0";
        }
        r += ")> {";
        for (size_t i = 0; i < e->elements.size(); ++i) {
            r += genExpr(e->elements[i].get());
            if (i + 1 < e->elements.size()) r += ", ";
        }
        r += "}";
        return r;
    }

    std::string CCodegen::genStructLit(StructLitExpr* e) {
        std::string r = e->typeName + "{";
        for (size_t i = 0; i < e->fields.size(); ++i) {
            r += genExpr(e->fields[i].value.get());
            if (i + 1 < e->fields.size()) r += ", ";
        }
        r += "}";
        return r;
    }

} // namespace mayami