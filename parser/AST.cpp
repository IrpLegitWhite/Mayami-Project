// src/parser/AST.cpp
#include "AST.h"

namespace mayami {

    // ============================================================
    // Утилиты
    // ============================================================
    std::string indentStr(int n) {
        return std::string(n * 2, ' ');
    }

    std::string typeKindName(TypeKind kind) {
        switch (kind) {
        case TypeKind::Void:   return "void";
        case TypeKind::I8:     return "i8";
        case TypeKind::I16:    return "i16";
        case TypeKind::I32:    return "i32";
        case TypeKind::I64:    return "i64";
        case TypeKind::U8:     return "u8";
        case TypeKind::U16:    return "u16";
        case TypeKind::U32:    return "u32";
        case TypeKind::U64:    return "u64";
        case TypeKind::F32:    return "f32";
        case TypeKind::F64:    return "f64";
        case TypeKind::Bool:   return "bool";
        case TypeKind::String: return "string";
        case TypeKind::Char:   return "char";
        case TypeKind::Named:  return "named";
        case TypeKind::Array:  return "array";
        case TypeKind::Slice:  return "slice";
        case TypeKind::Ref:    return "ref";
        case TypeKind::MutRef: return "mut-ref";
        case TypeKind::Ptr:    return "ptr";
        default:               return "?";
        }
    }

    std::string Type::toString() const {
        std::string s;
        switch (kind) {
        case TypeKind::Named:  s = name; break;
        case TypeKind::Ref:    s = "&" + (inner ? inner->toString() : "?"); break;
        case TypeKind::MutRef: s = "&mut " + (inner ? inner->toString() : "?"); break;
        case TypeKind::Ptr:    s = "*" + (inner ? inner->toString() : "?"); break;
        case TypeKind::Slice:  s = "[" + (inner ? inner->toString() : "?") + "]"; break;
        case TypeKind::Array:  s = "[" + (inner ? inner->toString() : "?") + "; "
            + std::to_string(arraySize) + "]"; break;
        default: return typeKindName(kind);
        }
        return s;
    }

    // ============================================================
    // Expr — toString
    // ============================================================
    std::string IntLitExpr::toString(int indent) const {
        return indentStr(indent) + "IntLit(" + std::to_string(value) + ")";
    }

    std::string FloatLitExpr::toString(int indent) const {
        return indentStr(indent) + "FloatLit(" + std::to_string(value) + ")";
    }

    std::string StringLitExpr::toString(int indent) const {
        return indentStr(indent) + "StringLit(\"" + value + "\")";
    }

    std::string CharLitExpr::toString(int indent) const {
        return indentStr(indent) + "CharLit('" + std::string(1, value) + "')";
    }

    std::string BoolLitExpr::toString(int indent) const {
        return indentStr(indent) + "BoolLit(" + (value ? "true" : "false") + ")";
    }

    std::string IdentifierExpr::toString(int indent) const {
        return indentStr(indent) + "Identifier(" + name + ")";
    }

    std::string UnaryExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Unary(" + op + ")\n";
        if (operand) s += operand->toString(indent + 1);
        return s;
    }

    std::string BinaryExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Binary(" + op + ")\n";
        if (left)  s += left->toString(indent + 1) + "\n";
        if (right) s += right->toString(indent + 1);
        return s;
    }

    std::string AssignExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Assign(" + op + ")\n";
        if (target) s += target->toString(indent + 1) + "\n";
        if (value)  s += value->toString(indent + 1);
        return s;
    }

    std::string CallExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Call\n";
        if (callee) s += callee->toString(indent + 1) + "\n";
        for (size_t i = 0; i < args.size(); ++i) {
            s += args[i]->toString(indent + 1);
            if (i + 1 < args.size()) s += "\n";
        }
        return s;
    }

    std::string FieldExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Field(" + field + ")\n";
        if (object) s += object->toString(indent + 1);
        return s;
    }

    std::string IndexExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Index\n";
        if (object) s += object->toString(indent + 1) + "\n";
        if (index)  s += index->toString(indent + 1);
        return s;
    }

    std::string GroupExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Group\n";
        if (inner) s += inner->toString(indent + 1);
        return s;
    }

    std::string ArrayLitExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "ArrayLit[" + std::to_string(elements.size()) + "]\n";
        for (size_t i = 0; i < elements.size(); ++i) {
            s += elements[i]->toString(indent + 1);
            if (i + 1 < elements.size()) s += "\n";
        }
        return s;
    }

    std::string StructLitExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "StructLit(" + typeName + ")\n";
        for (size_t i = 0; i < fields.size(); ++i) {
            s += indentStr(indent + 1) + fields[i].name + ":\n";
            s += fields[i].value->toString(indent + 2);
            if (i + 1 < fields.size()) s += "\n";
        }
        return s;
    }

    std::string PathExpr::toString(int indent) const {
        std::string s = indentStr(indent) + "Path(";
        for (size_t i = 0; i < segments.size(); ++i) {
            s += segments[i];
            if (i + 1 < segments.size()) s += "::";
        }
        s += ")";
        return s;
    }

    // ============================================================
    // Stmt — toString
    // ============================================================
    std::string LetStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "Let(" + (isMut ? "mut " : "") + name;
        if (declaredType) s += ": " + declaredType->toString();
        s += ")\n";
        if (init) s += init->toString(indent + 1);
        return s;
    }

    std::string ExprStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "ExprStmt\n";
        if (expr) s += expr->toString(indent + 1);
        return s;
    }

    std::string BlockStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "Block\n";
        for (size_t i = 0; i < statements.size(); ++i) {
            s += statements[i]->toString(indent + 1);
            if (i + 1 < statements.size()) s += "\n";
        }
        return s;
    }

    std::string IfStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "If\n";
        s += indentStr(indent + 1) + "Condition:\n";
        if (condition) s += condition->toString(indent + 2) + "\n";
        s += indentStr(indent + 1) + "Then:\n";
        if (thenBranch) s += thenBranch->toString(indent + 2);
        if (elseBranch) {
            s += "\n" + indentStr(indent + 1) + "Else:\n";
            s += elseBranch->toString(indent + 2);
        }
        return s;
    }

    std::string WhileStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "While\n";
        s += indentStr(indent + 1) + "Condition:\n";
        if (condition) s += condition->toString(indent + 2) + "\n";
        s += indentStr(indent + 1) + "Body:\n";
        if (body) s += body->toString(indent + 2);
        return s;
    }

    std::string ForStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "For(" + varName + ")\n";
        s += indentStr(indent + 1) + "Iterable:\n";
        if (iterable) s += iterable->toString(indent + 2) + "\n";
        s += indentStr(indent + 1) + "Body:\n";
        if (body) s += body->toString(indent + 2);
        return s;
    }

    std::string LoopStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "Loop\n";
        if (body) s += body->toString(indent + 1);
        return s;
    }

    std::string ReturnStmt::toString(int indent) const {
        std::string s = indentStr(indent) + "Return";
        if (value) {
            s += "\n" + value->toString(indent + 1);
        }
        return s;
    }

    std::string BreakStmt::toString(int indent) const {
        return indentStr(indent) + "Break";
    }

    std::string ContinueStmt::toString(int indent) const {
        return indentStr(indent) + "Continue";
    }

    // ============================================================
    // Decl — toString
    // ============================================================
    std::string FunctionDecl::toString(int indent) const {
        std::string s = indentStr(indent) + "FunctionDecl(" + name + ")\n";
        s += indentStr(indent + 1) + "Params: [";
        for (size_t i = 0; i < params.size(); ++i) {
            s += params[i].name;
            if (params[i].type) s += ": " + params[i].type->toString();
            if (i + 1 < params.size()) s += ", ";
        }
        s += "]\n";

        s += indentStr(indent + 1) + "ReturnType: ";
        s += returnType ? returnType->toString() : "void";
        s += "\n";

        if (body) {
            s += indentStr(indent + 1) + "Body:\n";
            s += body->toString(indent + 2);
        }
        return s;
    }

    std::string StructDecl::toString(int indent) const {
        std::string s = indentStr(indent) + "StructDecl(" + name + ")\n";
        for (size_t i = 0; i < fields.size(); ++i) {
            s += indentStr(indent + 1) + fields[i].name + ": ";
            s += fields[i].type ? fields[i].type->toString() : "?";
            if (i + 1 < fields.size()) s += "\n";
        }
        return s;
    }

    std::string EnumDecl::toString(int indent) const {
        std::string s = indentStr(indent) + "EnumDecl(" + name + ")\n";
        for (size_t i = 0; i < variants.size(); ++i) {
            s += indentStr(indent + 1) + variants[i].name;
            if (!variants[i].payload.empty()) {
                s += "(";
                for (size_t j = 0; j < variants[i].payload.size(); ++j) {
                    s += variants[i].payload[j]->toString();
                    if (j + 1 < variants[i].payload.size()) s += ", ";
                }
                s += ")";
            }
            if (i + 1 < variants.size()) s += "\n";
        }
        return s;
    }

    std::string ImplDecl::toString(int indent) const {
        std::string s = indentStr(indent) + "Impl(" + typeName + ")\n";
        for (size_t i = 0; i < methods.size(); ++i) {
            s += methods[i]->toString(indent + 1);
            if (i + 1 < methods.size()) s += "\n";
        }
        return s;
    }

    std::string UseDecl::toString(int indent) const {
        std::string s = indentStr(indent) + "Use(";
        for (size_t i = 0; i < path.size(); ++i) {
            s += path[i];
            if (i + 1 < path.size()) s += "::";
        }
        s += ")";
        return s;
    }

    // ============================================================
    // ★ ImportDecl — toString
    // ============================================================
    std::string ImportDecl::toString(int indent) const {
        return indentStr(indent) + "Import(\"" + path + "\")";
    }

    // ============================================================
    // Program
    // ============================================================
    std::string Program::toString() const {
        std::string s = "Program\n";
        for (size_t i = 0; i < declarations.size(); ++i) {
            s += declarations[i]->toString(1);
            if (i + 1 < declarations.size()) s += "\n";
        }
        return s;
    }

} // namespace mayami