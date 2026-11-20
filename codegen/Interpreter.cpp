// src/codegen/Interpreter.cpp
#include "Interpreter.h"
#include <cmath>
#include <cstdio>
#include <cctype>
#include <unordered_map>
#include <parser/Parser.h>
#include <lexer/Lexer.h>

namespace mayami {

    Interpreter::Interpreter() {
        m_globals = std::make_shared<Environment>(nullptr);
        m_env = m_globals;
    }

    void Interpreter::error(const std::string& msg) {
        m_errors.push_back(msg);
        if (m_error) m_error("[Runtime Error] " + msg);
    }

    bool Interpreter::run(Program* program) {
        if (!program) { error("Null program"); return false; }

        for (auto& decl : program->declarations) {
            if (decl->kind == DeclKind::Function) {
                auto fn = static_cast<FunctionDecl*>(decl.get());
                m_functions[fn->name] = fn;
            }
            else if (decl->kind == DeclKind::Struct) {
                auto st = static_cast<StructDecl*>(decl.get());
                StructDef def;
                def.name = st->name;
                def.fields = st->fields;
                m_structs[st->name] = def;
            }
            else if (decl->kind == DeclKind::Impl) {
                auto impl = static_cast<ImplDecl*>(decl.get());
                for (auto& method : impl->methods) {
                    m_methods[impl->typeName][method->name] = method.get();
                }
            }
        }

        auto it = m_functions.find("main");
        if (it == m_functions.end()) { error("No 'main' function found"); return false; }

        try {
            callFunction(it->second, {});
        }
        catch (ReturnException&) {}
        catch (const std::exception& e) {
            error(std::string("Uncaught exception: ") + e.what());
            return false;
        }
        return !hasErrors();
    }

    void Interpreter::visitFunctionDecl(FunctionDecl* d) { m_functions[d->name] = d; }
    void Interpreter::visitStructDecl(StructDecl* d) { (void)d; }
    void Interpreter::visitEnumDecl(EnumDecl* d) { (void)d; }
    void Interpreter::visitImplDecl(ImplDecl* d) { (void)d; }

    void Interpreter::visitStmt(Stmt* stmt) {
        if (!stmt) return;
        switch (stmt->kind) {
        case StmtKind::Let:      visitLetStmt(static_cast<LetStmt*>(stmt)); break;
        case StmtKind::ExprStmt: visitExprStmt(static_cast<ExprStmt*>(stmt)); break;
        case StmtKind::Block:    visitBlockStmt(static_cast<BlockStmt*>(stmt)); break;
        case StmtKind::If:       visitIfStmt(static_cast<IfStmt*>(stmt)); break;
        case StmtKind::While:    visitWhileStmt(static_cast<WhileStmt*>(stmt)); break;
        case StmtKind::For:      visitForStmt(static_cast<ForStmt*>(stmt)); break;
        case StmtKind::Loop:     visitLoopStmt(static_cast<LoopStmt*>(stmt)); break;
        case StmtKind::Return:   visitReturnStmt(static_cast<ReturnStmt*>(stmt)); break;
        case StmtKind::Break:    visitBreakStmt(static_cast<BreakStmt*>(stmt)); break;
        case StmtKind::Continue: visitContinueStmt(static_cast<ContinueStmt*>(stmt)); break;
        }
    }

    void Interpreter::visitLetStmt(LetStmt* stmt) {
        ValuePtr value = stmt->init ? visitExpr(stmt->init.get()) : makeNull();
        m_env->define(stmt->name, value);
    }
    void Interpreter::visitExprStmt(ExprStmt* stmt) { if (stmt->expr) visitExpr(stmt->expr.get()); }

    void Interpreter::visitBlockStmt(BlockStmt* stmt) {
        auto prev = m_env;
        m_env = std::make_shared<Environment>(prev);
        for (auto& s : stmt->statements) visitStmt(s.get());
        m_env = prev;
    }

    void Interpreter::visitIfStmt(IfStmt* stmt) {
        ValuePtr cond = visitExpr(stmt->condition.get());
        if (!cond->isBool()) { error("if condition must be bool"); return; }
        if (cond->b) visitStmt(stmt->thenBranch.get());
        else if (stmt->elseBranch) visitStmt(stmt->elseBranch.get());
    }

    void Interpreter::visitWhileStmt(WhileStmt* stmt) {
        while (true) {
            ValuePtr cond = visitExpr(stmt->condition.get());
            if (!cond->isBool()) { error("while condition must be bool"); return; }
            if (!cond->b) break;
            try { visitStmt(stmt->body.get()); }
            catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
    }

    // ★ For: диапазон или массив
    void Interpreter::visitForStmt(ForStmt* stmt) {
        // Диапазон a..b
        if (stmt->iterable->kind == ExprKind::Binary) {
            auto bin = static_cast<BinaryExpr*>(stmt->iterable.get());
            if (bin->op == "..") {
                ValuePtr startV = visitExpr(bin->left.get());
                ValuePtr endV = visitExpr(bin->right.get());
                if (!startV->isInt() || !endV->isInt()) {
                    error("for range must be int..int");
                    return;
                }
                auto prev = m_env;
                m_env = std::make_shared<Environment>(prev);
                for (long long i = startV->i; i < endV->i; ++i) {
                    m_env->define(stmt->varName, makeInt(i));
                    try { visitStmt(stmt->body.get()); }
                    catch (BreakException&) { break; }
                    catch (ContinueException&) { continue; }
                }
                m_env = prev;
                return;
            }
        }

        // Массив
        ValuePtr iterable = visitExpr(stmt->iterable.get());

        if (iterable->isArray()) {
            auto prev = m_env;
            m_env = std::make_shared<Environment>(prev);
            for (auto& elem : *iterable->elements) {
                m_env->define(stmt->varName, elem);
                try { visitStmt(stmt->body.get()); }
                catch (BreakException&) { break; }
                catch (ContinueException&) { continue; }
            }
            m_env = prev;
            return;
        }

        // Просто 0..N
        if (iterable->isInt()) {
            auto prev = m_env;
            m_env = std::make_shared<Environment>(prev);
            for (long long i = 0; i < iterable->i; ++i) {
                m_env->define(stmt->varName, makeInt(i));
                try { visitStmt(stmt->body.get()); }
                catch (BreakException&) { break; }
                catch (ContinueException&) { continue; }
            }
            m_env = prev;
            return;
        }

        error("for loop requires array or int range");
    }

    void Interpreter::visitLoopStmt(LoopStmt* stmt) {
        while (true) {
            try { visitStmt(stmt->body.get()); }
            catch (BreakException&) { break; }
            catch (ContinueException&) { continue; }
        }
    }

    void Interpreter::visitReturnStmt(ReturnStmt* stmt) {
        ValuePtr value = stmt->value ? visitExpr(stmt->value.get()) : makeNull();
        throw ReturnException{ value };
    }
    void Interpreter::visitBreakStmt(BreakStmt* s) { (void)s; throw BreakException{}; }
    void Interpreter::visitContinueStmt(ContinueStmt* s) { (void)s; throw ContinueException{}; }

    ValuePtr Interpreter::visitExpr(Expr* expr) {
        if (!expr) return makeNull();
        switch (expr->kind) {
        case ExprKind::IntLit:     return visitIntLit(static_cast<IntLitExpr*>(expr));
        case ExprKind::FloatLit:   return visitFloatLit(static_cast<FloatLitExpr*>(expr));
        case ExprKind::StringLit:  return visitStringLit(static_cast<StringLitExpr*>(expr));
        case ExprKind::CharLit:    return visitCharLit(static_cast<CharLitExpr*>(expr));
        case ExprKind::BoolLit:    return visitBoolLit(static_cast<BoolLitExpr*>(expr));
        case ExprKind::Identifier: return visitIdentifier(static_cast<IdentifierExpr*>(expr));
        case ExprKind::Unary:      return visitUnary(static_cast<UnaryExpr*>(expr));
        case ExprKind::Binary:     return visitBinary(static_cast<BinaryExpr*>(expr));
        case ExprKind::Assign:     return visitAssign(static_cast<AssignExpr*>(expr));
        case ExprKind::Call:       return visitCall(static_cast<CallExpr*>(expr));
        case ExprKind::Field:      return visitField(static_cast<FieldExpr*>(expr));
        case ExprKind::Index:      return visitIndex(static_cast<IndexExpr*>(expr));
        case ExprKind::Group:      return visitGroup(static_cast<GroupExpr*>(expr));
        case ExprKind::ArrayLit:   return visitArrayLit(static_cast<ArrayLitExpr*>(expr));
        case ExprKind::StructLit:  return visitStructLit(static_cast<StructLitExpr*>(expr));
        default: error("Unsupported expr"); return makeNull();
        }
    }

    ValuePtr Interpreter::visitIntLit(IntLitExpr* e) { return makeInt(e->value); }
    ValuePtr Interpreter::visitFloatLit(FloatLitExpr* e) { return makeFloat(e->value); }
    ValuePtr Interpreter::visitStringLit(StringLitExpr* e) { return makeString(e->value); }
    ValuePtr Interpreter::visitCharLit(CharLitExpr* e) { return makeChar(e->value); }
    ValuePtr Interpreter::visitBoolLit(BoolLitExpr* e) { return makeBool(e->value); }

    ValuePtr Interpreter::visitIdentifier(IdentifierExpr* e) {
        ValuePtr v = m_env->get(e->name);
        if (!v) { error("Undefined variable: '" + e->name + "'"); return makeNull(); }
        return v;
    }

    ValuePtr Interpreter::visitUnary(UnaryExpr* e) {
        ValuePtr op = visitExpr(e->operand.get());
        if (e->op == "-") {
            if (op->isInt())   return makeInt(-op->i);
            if (op->isFloat()) return makeFloat(-op->f);
            error("Unary '-' requires number"); return makeNull();
        }
        if (e->op == "!") {
            if (!op->isBool()) { error("Unary '!' requires bool"); return makeNull(); }
            return makeBool(!op->b);
        }
        if (e->op == "~") {
            if (!op->isInt()) { error("Unary '~' requires int"); return makeNull(); }
            return makeInt(~op->i);
        }
        error("Unknown unary op: " + e->op); return makeNull();
    }

    ValuePtr Interpreter::visitBinary(BinaryExpr* e) {
        if (e->op == "..") { error("Range only in for"); return makeNull(); }

        ValuePtr l = visitExpr(e->left.get());
        ValuePtr r = visitExpr(e->right.get());
        const std::string& op = e->op;

        if (op == "&&") {
            if (!l->isBool() || !r->isBool()) { error("'&&' requires bool"); return makeNull(); }
            return makeBool(l->b && r->b);
        }
        if (op == "||") {
            if (!l->isBool() || !r->isBool()) { error("'||' requires bool"); return makeNull(); }
            return makeBool(l->b || r->b);
        }
        if (op == "+" && (l->isString() || r->isString()))
            return makeString(l->toString() + r->toString());

        if (op == "==") return makeBool(l->toString() == r->toString());
        if (op == "!=") return makeBool(l->toString() != r->toString());
        if (op == "<")  return makeBool(l->toNumber() < r->toNumber());
        if (op == ">")  return makeBool(l->toNumber() > r->toNumber());
        if (op == "<=") return makeBool(l->toNumber() <= r->toNumber());
        if (op == ">=") return makeBool(l->toNumber() >= r->toNumber());

        if (!l->isNumber() || !r->isNumber()) { error("Arithmetic requires numbers"); return makeNull(); }
        bool bothInt = l->isInt() && r->isInt();

        if (op == "+") return bothInt ? makeInt(l->i + r->i) : makeFloat(l->toNumber() + r->toNumber());
        if (op == "-") return bothInt ? makeInt(l->i - r->i) : makeFloat(l->toNumber() - r->toNumber());
        if (op == "*") return bothInt ? makeInt(l->i * r->i) : makeFloat(l->toNumber() * r->toNumber());
        if (op == "/") {
            double rr = r->toNumber();
            if (rr == 0.0) { error("Division by zero"); return makeNull(); }
            return bothInt ? makeInt(l->i / r->i) : makeFloat(l->toNumber() / rr);
        }
        if (op == "%") {
            if (!bothInt) { error("'%' requires int"); return makeNull(); }
            if (r->i == 0) { error("Modulo by zero"); return makeNull(); }
            return makeInt(l->i % r->i);
        }
        error("Unknown binary op: " + op); return makeNull();
    }

    ValuePtr Interpreter::visitAssign(AssignExpr* e) {
        // Присваивание полю
        if (e->target->kind == ExprKind::Field) {
            auto field = static_cast<FieldExpr*>(e->target.get());
            ValuePtr obj = visitExpr(field->object.get());
            if (!obj->isStruct() || !obj->fields) { error("Field assignment requires struct"); return makeNull(); }
            ValuePtr v = visitExpr(e->value.get());
            (*obj->fields)[field->field] = v;
            return v;
        }

        // Присваивание элементу массива
        if (e->target->kind == ExprKind::Index) {
            auto idx = static_cast<IndexExpr*>(e->target.get());
            ValuePtr obj = visitExpr(idx->object.get());
            ValuePtr i = visitExpr(idx->index.get());
            if (!obj->isArray()) { error("Index assignment requires array"); return makeNull(); }
            if (!i->isInt()) { error("Array index must be int"); return makeNull(); }
            long long index = i->i;
            if (index < 0 || index >= (long long)obj->elements->size()) {
                error("Array index out of bounds: " + std::to_string(index));
                return makeNull();
            }
            ValuePtr v = visitExpr(e->value.get());
            (*obj->elements)[(size_t)index] = v;
            return v;
        }

        if (e->target->kind != ExprKind::Identifier) {
            error("Assignment target must be identifier/field/index");
            return makeNull();
        }

        auto id = static_cast<IdentifierExpr*>(e->target.get());
        ValuePtr v = visitExpr(e->value.get());

        if (e->op != "=") {
            ValuePtr cur = m_env->get(id->name);
            if (!cur) { error("Undefined variable: '" + id->name + "'"); return makeNull(); }
            std::string binOp = e->op.substr(0, 1);
            if (binOp == "+") {
                if (cur->isString() || v->isString()) v = makeString(cur->toString() + v->toString());
                else if (cur->isInt() && v->isInt()) v = makeInt(cur->i + v->i);
                else v = makeFloat(cur->toNumber() + v->toNumber());
            }
            else if (binOp == "-") {
                if (cur->isInt() && v->isInt()) v = makeInt(cur->i - v->i);
                else v = makeFloat(cur->toNumber() - v->toNumber());
            }
            else if (binOp == "*") {
                if (cur->isInt() && v->isInt()) v = makeInt(cur->i * v->i);
                else v = makeFloat(cur->toNumber() * v->toNumber());
            }
            else if (binOp == "/") {
                if (v->toNumber() == 0.0) { error("Division by zero"); return makeNull(); }
                if (cur->isInt() && v->isInt()) v = makeInt(cur->i / v->i);
                else v = makeFloat(cur->toNumber() / v->toNumber());
            }
            else if (binOp == "%") {
                if (!cur->isInt() || !v->isInt()) { error("'%' requires int"); return makeNull(); }
                v = makeInt(cur->i % v->i);
            }
        }

        if (!m_env->assign(id->name, v)) { error("Undefined variable: '" + id->name + "'"); return makeNull(); }
        return v;
    }

    // ★ Алиасы
    static const std::unordered_map<std::string, std::string>& getAliases() {
        static const std::unordered_map<std::string, std::string> aliases = {
            { "p", "println" }, { "w", "print" }, { "e", "eprintln" }, { "ew", "eprint" },
            { "s", "sqrt" }, { "a", "abs" },
            { "si", "sin" }, { "co", "cos" }, { "ta", "tan" },
            { "pw", "pow" }, { "fl", "floor" }, { "ce", "ceil" }, { "ro", "round" },
            { "mn", "min" }, { "mx", "max" },
        };
        return aliases;
    }

    ValuePtr Interpreter::visitCall(CallExpr* expr) {
        // obj.method(args)
        if (expr->callee->kind == ExprKind::Field) {
            auto field = static_cast<FieldExpr*>(expr->callee.get());
            ValuePtr obj = visitExpr(field->object.get());
            const std::string& methodName = field->field;

            // ★ Строковые методы
            if (obj->isString() || obj->isChar()) {
                std::vector<ValuePtr> args;
                for (auto& a : expr->args) args.push_back(visitExpr(a.get()));
                return callStringMethod(obj, methodName, args);
            }

            // ★ Методы массивов
            if (obj->isArray()) {
                std::vector<ValuePtr> args;
                for (auto& a : expr->args) args.push_back(visitExpr(a.get()));

                if (methodName == "len" || methodName == "length")
                    return makeInt((long long)obj->elements->size());
                if (methodName == "push" && args.size() == 1) {
                    obj->elements->push_back(args[0]);
                    return makeNull();
                }
                if (methodName == "pop") {
                    if (obj->elements->empty()) return makeNull();
                    ValuePtr last = obj->elements->back();
                    obj->elements->pop_back();
                    return last;
                }
                if (methodName == "clear") { obj->elements->clear(); return makeNull(); }
                if (methodName == "isEmpty" || methodName == "empty")
                    return makeBool(obj->elements->empty());

                error("Unknown array method: '" + methodName + "'");
                return makeNull();
            }

            // Методы структур
            if (!obj->isStruct()) { error("Method call requires struct/string/array"); return makeNull(); }

            const std::string& typeName = obj->structName;
            auto typeIt = m_methods.find(typeName);
            if (typeIt == m_methods.end()) { error("Struct '" + typeName + "' has no methods"); return makeNull(); }
            auto mIt = typeIt->second.find(methodName);
            if (mIt == typeIt->second.end()) {
                error("Struct '" + typeName + "' has no method '" + methodName + "'");
                return makeNull();
            }

            FunctionDecl* method = mIt->second;
            std::vector<ValuePtr> args;
            if (!method->params.empty()) {
                const std::string& first = method->params[0].name;
                if (first == "self" || first == "&self" || first == "mut self" || first == "&mut self")
                    args.push_back(obj);
            }
            for (auto& a : expr->args) args.push_back(visitExpr(a.get()));
            return callFunction(method, args);
        }

        if (expr->callee->kind != ExprKind::Identifier) {
            error("Call target must be identifier or method");
            return makeNull();
        }

        auto id = static_cast<IdentifierExpr*>(expr->callee.get());
        std::string name = id->name;

        std::vector<ValuePtr> args;
        for (auto& a : expr->args) args.push_back(visitExpr(a.get()));

        // Алиасы
        const auto& aliases = getAliases();
        auto aliasIt = aliases.find(name);
        if (aliasIt != aliases.end()) name = aliasIt->second;

        static const std::vector<std::string> builtins = {
            "println", "print", "eprintln", "eprint",
            "sqrt", "abs", "sin", "cos", "tan", "pow",
            "floor", "ceil", "round", "min", "max",
        };
        for (const auto& b : builtins) {
            if (name == b) return callBuiltin(name, args);
        }

        auto it = m_functions.find(name);
        if (it == m_functions.end()) { error("Undefined function: '" + name + "'"); return makeNull(); }
        return callFunction(it->second, args);
    }

    // ★ Строковые методы
    ValuePtr Interpreter::callStringMethod(ValuePtr obj, const std::string& method,
        const std::vector<ValuePtr>& args) {
        const std::string& s = obj->s;

        if (method == "len" || method == "length")
            return makeInt((long long)s.length());

        if (method == "upper" || method == "toUpper") {
            std::string r = s;
            for (auto& c : r) c = (char)std::toupper((unsigned char)c);
            return makeString(r);
        }
        if (method == "lower" || method == "toLower") {
            std::string r = s;
            for (auto& c : r) c = (char)std::tolower((unsigned char)c);
            return makeString(r);
        }
        if ((method == "substr" || method == "substring") && args.size() == 2) {
            long long start = args[0]->i;
            long long len = args[1]->i;
            if (start < 0) start = 0;
            if (start >= (long long)s.length()) return makeString("");
            if (start + len > (long long)s.length()) len = (long long)s.length() - start;
            return makeString(s.substr((size_t)start, (size_t)len));
        }
        if (method == "find" && args.size() == 1) {
            size_t pos = s.find(args[0]->toString());
            return makeInt(pos == std::string::npos ? -1 : (long long)pos);
        }
        if (method == "contains" && args.size() == 1)
            return makeBool(s.find(args[0]->toString()) != std::string::npos);
        if (method == "replace" && args.size() == 2) {
            std::string from = args[0]->toString();
            std::string to = args[1]->toString();
            std::string r = s;
            if (!from.empty()) {
                size_t pos = 0;
                while ((pos = r.find(from, pos)) != std::string::npos) {
                    r.replace(pos, from.length(), to);
                    pos += to.length();
                }
            }
            return makeString(r);
        }
        if (method == "startsWith" && args.size() == 1) {
            std::string prefix = args[0]->toString();
            return makeBool(s.size() >= prefix.size() &&
                s.compare(0, prefix.size(), prefix) == 0);
        }
        if (method == "endsWith" && args.size() == 1) {
            std::string suffix = args[0]->toString();
            return makeBool(s.size() >= suffix.size() &&
                s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
        }
        if (method == "trim") {
            size_t start = 0;
            while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
            size_t end = s.size();
            while (end > start && std::isspace((unsigned char)s[end - 1])) --end;
            return makeString(s.substr(start, end - start));
        }

        error("Unknown string method: '" + method + "'");
        return makeNull();
    }

    ValuePtr Interpreter::callBuiltin(const std::string& name,
        const std::vector<ValuePtr>& args) {
        if (name == "println" || name == "print" || name == "eprintln" || name == "eprint") {
            std::string s;
            for (size_t i = 0; i < args.size(); ++i) {
                s += args[i]->toString();
                if (i + 1 < args.size()) s += " ";
            }
            if (m_output) m_output(s);
            return makeNull();
        }

        if (name == "sqrt" && args.size() == 1) return makeFloat(std::sqrt(args[0]->toNumber()));
        if (name == "abs" && args.size() == 1) {
            if (args[0]->isInt()) return makeInt(args[0]->i < 0 ? -args[0]->i : args[0]->i);
            return makeFloat(std::fabs(args[0]->toNumber()));
        }
        if (name == "sin" && args.size() == 1) return makeFloat(std::sin(args[0]->toNumber()));
        if (name == "cos" && args.size() == 1) return makeFloat(std::cos(args[0]->toNumber()));
        if (name == "tan" && args.size() == 1) return makeFloat(std::tan(args[0]->toNumber()));
        if (name == "pow" && args.size() == 2) return makeFloat(std::pow(args[0]->toNumber(), args[1]->toNumber()));
        if (name == "floor" && args.size() == 1) return makeInt((long long)std::floor(args[0]->toNumber()));
        if (name == "ceil" && args.size() == 1) return makeInt((long long)std::ceil(args[0]->toNumber()));
        if (name == "round" && args.size() == 1) return makeInt((long long)std::round(args[0]->toNumber()));
        if (name == "min" && args.size() == 2) {
            if (args[0]->isInt() && args[1]->isInt()) return makeInt(args[0]->i < args[1]->i ? args[0]->i : args[1]->i);
            return makeFloat(std::min(args[0]->toNumber(), args[1]->toNumber()));
        }
        if (name == "max" && args.size() == 2) {
            if (args[0]->isInt() && args[1]->isInt()) return makeInt(args[0]->i > args[1]->i ? args[0]->i : args[1]->i);
            return makeFloat(std::max(args[0]->toNumber(), args[1]->toNumber()));
        }

        error("Unknown builtin: " + name);
        return makeNull();
    }

    ValuePtr Interpreter::callFunction(FunctionDecl* fn, const std::vector<ValuePtr>& args) {
        if (m_callDepth >= MAX_CALL_DEPTH) {
            error("Stack overflow");
            throw ReturnException{ makeNull() };
        }
        if (args.size() != fn->params.size()) {
            error("Function '" + fn->name + "' expects " + std::to_string(fn->params.size()) +
                " args, got " + std::to_string(args.size()));
            return makeNull();
        }
        auto prev = m_env;
        m_env = std::make_shared<Environment>(prev);
        for (size_t i = 0; i < fn->params.size(); ++i)
            m_env->define(fn->params[i].name, args[i]);

        ++m_callDepth;
        ValuePtr result = makeNull();
        try { if (fn->body) visitBlockStmt(fn->body.get()); }
        catch (ReturnException& e) { result = e.value; }
        --m_callDepth;
        m_env = prev;
        return result;
    }

    ValuePtr Interpreter::visitStructLit(StructLitExpr* expr) {
        auto it = m_structs.find(expr->typeName);
        if (it == m_structs.end()) { error("Undefined struct: '" + expr->typeName + "'"); return makeNull(); }
        const StructDef& def = it->second;
        ValuePtr value = makeStruct(expr->typeName);
        for (const auto& f : def.fields) (*value->fields)[f.name] = makeNull();
        for (const auto& fi : expr->fields) {
            bool found = false;
            for (const auto& df : def.fields) { if (df.name == fi.name) { found = true; break; } }
            if (!found) {
                error("Struct '" + expr->typeName + "' has no field '" + fi.name + "'");
                return makeNull();
            }
            (*value->fields)[fi.name] = visitExpr(fi.value.get());
        }
        return value;
    }

    ValuePtr Interpreter::visitField(FieldExpr* expr) {
        ValuePtr obj = visitExpr(expr->object.get());
        if (!obj->isStruct() || !obj->fields) { error("Field access requires struct"); return makeNull(); }
        auto it = obj->fields->find(expr->field);
        if (it == obj->fields->end()) {
            error("Struct '" + obj->structName + "' has no field '" + expr->field + "'");
            return makeNull();
        }
        return it->second;
    }

    ValuePtr Interpreter::visitIndex(IndexExpr* expr) {
        ValuePtr obj = visitExpr(expr->object.get());
        ValuePtr idx = visitExpr(expr->index.get());
        if (!idx->isInt()) { error("Index must be int"); return makeNull(); }
        long long i = idx->i;

        if (obj->isArray()) {
            if (i < 0 || i >= (long long)obj->elements->size()) {
                error("Array index out of bounds: " + std::to_string(i));
                return makeNull();
            }
            return (*obj->elements)[(size_t)i];
        }
        if (obj->isString()) {
            if (i < 0 || i >= (long long)obj->s.length()) {
                error("String index out of bounds: " + std::to_string(i));
                return makeNull();
            }
            return makeChar(obj->s[(size_t)i]);
        }
        error("Index requires array or string");
        return makeNull();
    }

    ValuePtr Interpreter::visitGroup(GroupExpr* expr) { return visitExpr(expr->inner.get()); }

    ValuePtr Interpreter::visitArrayLit(ArrayLitExpr* expr) {
        ValuePtr arr = makeArray();
        for (auto& e : expr->elements) arr->elements->push_back(visitExpr(e.get()));
        return arr;
    }

} // namespace mayami