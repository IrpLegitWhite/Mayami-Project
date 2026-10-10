// src/parser/AST.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace mayami {

    // ============================================================
    // Forward declarations
    // ============================================================
    struct Expr;
    struct Stmt;
    struct Decl;

    using ExprPtr = std::unique_ptr<Expr>;
    using StmtPtr = std::unique_ptr<Stmt>;
    using DeclPtr = std::unique_ptr<Decl>;

    // ============================================================
    // Типы (для AST)
    // ============================================================
    enum class TypeKind {
        Unknown,
        Void,
        I8, I16, I32, I64,
        U8, U16, U32, U64,
        F32, F64,
        Bool,
        String,
        Char,
        // Пользовательские типы
        Named,      // "Point", "MyStruct"
        Array,      // [T; N]
        Slice,      // [T]
        Ref,        // &T
        MutRef,     // &mut T
        Ptr,        // *T
    };

    struct Type {
        TypeKind kind = TypeKind::Unknown;
        std::string name;           // для Named
        std::shared_ptr<Type> inner; // для Ref, Array, Slice
        size_t arraySize = 0;       // для Array

        std::string toString() const;
    };

    using TypePtr = std::shared_ptr<Type>;

    // ============================================================
    // Expr — выражения
    // ============================================================
    enum class ExprKind {
        // Литералы
        IntLit,
        FloatLit,
        StringLit,
        CharLit,
        BoolLit,

        // Идентификатор
        Identifier,

        // Унарные
        Unary,      // -x, !x, ~x

        // Бинарные
        Binary,     // a + b, a == b, ...

        // Присваивание
        Assign,     // x = 5

        // Вызов функции
        Call,       // foo(a, b)

        // Обращение к полю
        Field,      // obj.field

        // Индексация
        Index,      // arr[i]

        // Группа
        Group,      // (expr)

        // Блок как выражение
        Block,      // { stmt; ...; expr }

        // If как выражение
        IfExpr,

        // Массив
        ArrayLit,   // [1, 2, 3]

        // Структура
        StructLit,  // Point { x: 1, y: 2 }

        // Path (foo::bar::baz)
        Path,
    };

    struct Expr {
        ExprKind kind;
        int line = 0;
        int column = 0;
        TypePtr type;   // заполняется в sema

        Expr(ExprKind k) : kind(k) {}
        virtual ~Expr() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    // ─── IntLit ─────────────────────────────────────────────
    struct IntLitExpr : Expr {
        long long value;
        IntLitExpr() : Expr(ExprKind::IntLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── FloatLit ───────────────────────────────────────────
    struct FloatLitExpr : Expr {
        double value;
        FloatLitExpr() : Expr(ExprKind::FloatLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── StringLit ──────────────────────────────────────────
    struct StringLitExpr : Expr {
        std::string value;
        StringLitExpr() : Expr(ExprKind::StringLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── CharLit ────────────────────────────────────────────
    struct CharLitExpr : Expr {
        char value;
        CharLitExpr() : Expr(ExprKind::CharLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── BoolLit ────────────────────────────────────────────
    struct BoolLitExpr : Expr {
        bool value;
        BoolLitExpr() : Expr(ExprKind::BoolLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Identifier ─────────────────────────────────────────
    struct IdentifierExpr : Expr {
        std::string name;
        IdentifierExpr() : Expr(ExprKind::Identifier) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Unary ──────────────────────────────────────────────
    struct UnaryExpr : Expr {
        std::string op;     // "-", "!", "~"
        ExprPtr operand;
        UnaryExpr() : Expr(ExprKind::Unary) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Binary ─────────────────────────────────────────────
    struct BinaryExpr : Expr {
        std::string op;     // "+", "-", "*", "/", "%", "==", "!=", ...
        ExprPtr left;
        ExprPtr right;
        BinaryExpr() : Expr(ExprKind::Binary) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Assign ─────────────────────────────────────────────
    struct AssignExpr : Expr {
        std::string op;     // "=", "+=", "-=", ...
        ExprPtr target;
        ExprPtr value;
        AssignExpr() : Expr(ExprKind::Assign) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Call ───────────────────────────────────────────────
    struct CallExpr : Expr {
        ExprPtr callee;
        std::vector<ExprPtr> args;
        CallExpr() : Expr(ExprKind::Call) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Field ──────────────────────────────────────────────
    struct FieldExpr : Expr {
        ExprPtr object;
        std::string field;
        FieldExpr() : Expr(ExprKind::Field) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Index ──────────────────────────────────────────────
    struct IndexExpr : Expr {
        ExprPtr object;
        ExprPtr index;
        IndexExpr() : Expr(ExprKind::Index) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Group ──────────────────────────────────────────────
    struct GroupExpr : Expr {
        ExprPtr inner;
        GroupExpr() : Expr(ExprKind::Group) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── ArrayLit ───────────────────────────────────────────
    struct ArrayLitExpr : Expr {
        std::vector<ExprPtr> elements;
        ArrayLitExpr() : Expr(ExprKind::ArrayLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── StructLit ──────────────────────────────────────────
    struct StructLitField {
        std::string name;
        ExprPtr value;
    };

    struct StructLitExpr : Expr {
        std::string typeName;
        std::vector<StructLitField> fields;
        StructLitExpr() : Expr(ExprKind::StructLit) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Path (foo::bar::baz) ───────────────────────────────
    struct PathExpr : Expr {
        std::vector<std::string> segments;
        PathExpr() : Expr(ExprKind::Path) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Stmt — инструкции
    // ============================================================
    enum class StmtKind {
        Let,
        ExprStmt,
        Block,
        If,
        While,
        For,
        Loop,
        Return,
        Break,
        Continue,
    };

    struct Stmt {
        StmtKind kind;
        int line = 0;
        int column = 0;

        Stmt(StmtKind k) : kind(k) {}
        virtual ~Stmt() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    // ─── Let ────────────────────────────────────────────────
    struct LetStmt : Stmt {
        std::string name;
        bool isMut = false;
        TypePtr declaredType;   // может быть null (вывод)
        ExprPtr init;           // может быть null

        LetStmt() : Stmt(StmtKind::Let) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── ExprStmt ───────────────────────────────────────────
    struct ExprStmt : Stmt {
        ExprPtr expr;
        ExprStmt() : Stmt(StmtKind::ExprStmt) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Block ──────────────────────────────────────────────
    struct BlockStmt : Stmt {
        std::vector<StmtPtr> statements;
        BlockStmt() : Stmt(StmtKind::Block) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── If ─────────────────────────────────────────────────
    struct IfStmt : Stmt {
        ExprPtr condition;
        StmtPtr thenBranch;
        StmtPtr elseBranch;     // может быть null, или IfStmt, или Block
        IfStmt() : Stmt(StmtKind::If) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── While ──────────────────────────────────────────────
    struct WhileStmt : Stmt {
        ExprPtr condition;
        StmtPtr body;
        WhileStmt() : Stmt(StmtKind::While) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── For ────────────────────────────────────────────────
    struct ForStmt : Stmt {
        std::string varName;
        ExprPtr iterable;
        StmtPtr body;
        ForStmt() : Stmt(StmtKind::For) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Loop ───────────────────────────────────────────────
    struct LoopStmt : Stmt {
        StmtPtr body;
        LoopStmt() : Stmt(StmtKind::Loop) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Return ─────────────────────────────────────────────
    struct ReturnStmt : Stmt {
        ExprPtr value;      // может быть null
        ReturnStmt() : Stmt(StmtKind::Return) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Break ──────────────────────────────────────────────
    struct BreakStmt : Stmt {
        BreakStmt() : Stmt(StmtKind::Break) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Continue ───────────────────────────────────────────
    struct ContinueStmt : Stmt {
        ContinueStmt() : Stmt(StmtKind::Continue) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Decl — объявления
    // ============================================================
    enum class DeclKind {
        Function,
        Struct,
        Enum,
        Impl,
        Use,
    };

    struct Decl {
        DeclKind kind;
        int line = 0;
        int column = 0;

        Decl(DeclKind k) : kind(k) {}
        virtual ~Decl() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    // ─── Param ──────────────────────────────────────────────
    struct Param {
        std::string name;
        TypePtr type;
    };

    // ─── Function ───────────────────────────────────────────
    struct FunctionDecl : Decl {
        std::string name;
        std::vector<Param> params;
        TypePtr returnType;
        std::unique_ptr<BlockStmt> body;

        FunctionDecl() : Decl(DeclKind::Function) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Struct field ───────────────────────────────────────
    struct StructField {
        std::string name;
        TypePtr type;
    };

    // ─── Struct ─────────────────────────────────────────────
    struct StructDecl : Decl {
        std::string name;
        std::vector<StructField> fields;

        StructDecl() : Decl(DeclKind::Struct) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Enum variant ───────────────────────────────────────
    struct EnumVariant {
        std::string name;
        std::vector<TypePtr> payload;   // пусто = unit variant
    };

    // ─── Enum ───────────────────────────────────────────────
    struct EnumDecl : Decl {
        std::string name;
        std::vector<EnumVariant> variants;

        EnumDecl() : Decl(DeclKind::Enum) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Impl ───────────────────────────────────────────────
    struct ImplDecl : Decl {
        std::string typeName;
        std::vector<std::unique_ptr<FunctionDecl>> methods;

        ImplDecl() : Decl(DeclKind::Impl) {}
        std::string toString(int indent = 0) const override;
    };

    // ─── Use ────────────────────────────────────────────────
    struct UseDecl : Decl {
        std::vector<std::string> path;
        UseDecl() : Decl(DeclKind::Use) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Program — корень дерева
    // ============================================================
    struct Program {
        std::vector<DeclPtr> declarations;
        std::string toString() const;
    };

    // ============================================================
    // Утилиты
    // ============================================================
    std::string indentStr(int n);
    std::string typeKindName(TypeKind kind);

} // namespace mayami