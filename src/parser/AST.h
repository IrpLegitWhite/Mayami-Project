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
        Named,
        Array,
        Slice,
        Ref,
        MutRef,
        Ptr,
    };

    struct Type {
        TypeKind kind = TypeKind::Unknown;
        std::string name;
        std::shared_ptr<Type> inner;
        size_t arraySize = 0;

        std::string toString() const;
    };

    using TypePtr = std::shared_ptr<Type>;

    // ============================================================
    // Expr
    // ============================================================
    enum class ExprKind {
        IntLit, FloatLit, StringLit, CharLit, BoolLit,
        Identifier,
        Unary,
        Binary,
        Assign,
        Call,
        Field,
        Index,
        Group,
        Block,
        IfExpr,
        ArrayLit,
        StructLit,
        Path,
    };

    struct Expr {
        ExprKind kind;
        int line = 0;
        int column = 0;
        TypePtr type;

        Expr(ExprKind k) : kind(k) {}
        virtual ~Expr() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct IntLitExpr : Expr {
        long long value;
        IntLitExpr() : Expr(ExprKind::IntLit) {}
        std::string toString(int indent = 0) const override;
    };

    struct FloatLitExpr : Expr {
        double value;
        FloatLitExpr() : Expr(ExprKind::FloatLit) {}
        std::string toString(int indent = 0) const override;
    };

    struct StringLitExpr : Expr {
        std::string value;
        StringLitExpr() : Expr(ExprKind::StringLit) {}
        std::string toString(int indent = 0) const override;
    };

    struct CharLitExpr : Expr {
        char value;
        CharLitExpr() : Expr(ExprKind::CharLit) {}
        std::string toString(int indent = 0) const override;
    };

    struct BoolLitExpr : Expr {
        bool value;
        BoolLitExpr() : Expr(ExprKind::BoolLit) {}
        std::string toString(int indent = 0) const override;
    };

    struct IdentifierExpr : Expr {
        std::string name;
        IdentifierExpr() : Expr(ExprKind::Identifier) {}
        std::string toString(int indent = 0) const override;
    };

    struct UnaryExpr : Expr {
        std::string op;
        ExprPtr operand;
        UnaryExpr() : Expr(ExprKind::Unary) {}
        std::string toString(int indent = 0) const override;
    };

    struct BinaryExpr : Expr {
        std::string op;
        ExprPtr left;
        ExprPtr right;
        BinaryExpr() : Expr(ExprKind::Binary) {}
        std::string toString(int indent = 0) const override;
    };

    struct AssignExpr : Expr {
        std::string op;
        ExprPtr target;
        ExprPtr value;
        AssignExpr() : Expr(ExprKind::Assign) {}
        std::string toString(int indent = 0) const override;
    };

    struct CallExpr : Expr {
        ExprPtr callee;
        std::vector<ExprPtr> args;
        CallExpr() : Expr(ExprKind::Call) {}
        std::string toString(int indent = 0) const override;
    };

    struct FieldExpr : Expr {
        ExprPtr object;
        std::string field;
        FieldExpr() : Expr(ExprKind::Field) {}
        std::string toString(int indent = 0) const override;
    };

    struct IndexExpr : Expr {
        ExprPtr object;
        ExprPtr index;
        IndexExpr() : Expr(ExprKind::Index) {}
        std::string toString(int indent = 0) const override;
    };

    struct GroupExpr : Expr {
        ExprPtr inner;
        GroupExpr() : Expr(ExprKind::Group) {}
        std::string toString(int indent = 0) const override;
    };

    struct ArrayLitExpr : Expr {
        std::vector<ExprPtr> elements;
        ArrayLitExpr() : Expr(ExprKind::ArrayLit) {}
        std::string toString(int indent = 0) const override;
    };

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

    struct PathExpr : Expr {
        std::vector<std::string> segments;
        PathExpr() : Expr(ExprKind::Path) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Stmt
    // ============================================================
    enum class StmtKind {
        Let, ExprStmt, Block, If, While, For, Loop,
        Return, Break, Continue,
    };

    struct Stmt {
        StmtKind kind;
        int line = 0;
        int column = 0;

        Stmt(StmtKind k) : kind(k) {}
        virtual ~Stmt() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct LetStmt : Stmt {
        std::string name;
        bool isMut = false;
        TypePtr declaredType;
        ExprPtr init;

        LetStmt() : Stmt(StmtKind::Let) {}
        std::string toString(int indent = 0) const override;
    };

    struct ExprStmt : Stmt {
        ExprPtr expr;
        ExprStmt() : Stmt(StmtKind::ExprStmt) {}
        std::string toString(int indent = 0) const override;
    };

    struct BlockStmt : Stmt {
        std::vector<StmtPtr> statements;
        BlockStmt() : Stmt(StmtKind::Block) {}
        std::string toString(int indent = 0) const override;
    };

    struct IfStmt : Stmt {
        ExprPtr condition;
        StmtPtr thenBranch;
        StmtPtr elseBranch;
        IfStmt() : Stmt(StmtKind::If) {}
        std::string toString(int indent = 0) const override;
    };

    struct WhileStmt : Stmt {
        ExprPtr condition;
        StmtPtr body;
        WhileStmt() : Stmt(StmtKind::While) {}
        std::string toString(int indent = 0) const override;
    };

    struct ForStmt : Stmt {
        std::string varName;
        ExprPtr iterable;
        StmtPtr body;
        ForStmt() : Stmt(StmtKind::For) {}
        std::string toString(int indent = 0) const override;
    };

    struct LoopStmt : Stmt {
        StmtPtr body;
        LoopStmt() : Stmt(StmtKind::Loop) {}
        std::string toString(int indent = 0) const override;
    };

    struct ReturnStmt : Stmt {
        ExprPtr value;
        ReturnStmt() : Stmt(StmtKind::Return) {}
        std::string toString(int indent = 0) const override;
    };

    struct BreakStmt : Stmt {
        BreakStmt() : Stmt(StmtKind::Break) {}
        std::string toString(int indent = 0) const override;
    };

    struct ContinueStmt : Stmt {
        ContinueStmt() : Stmt(StmtKind::Continue) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Decl
    // ============================================================
    enum class DeclKind {
        Function,
        Struct,
        Enum,
        Impl,
        Use,
        Import,   // ★
    };

    struct Decl {
        DeclKind kind;
        int line = 0;
        int column = 0;

        Decl(DeclKind k) : kind(k) {}
        virtual ~Decl() = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct Param {
        std::string name;
        TypePtr type;
    };

    struct FunctionDecl : Decl {
        std::string name;
        std::vector<Param> params;
        TypePtr returnType;
        std::unique_ptr<BlockStmt> body;

        FunctionDecl() : Decl(DeclKind::Function) {}
        std::string toString(int indent = 0) const override;
    };

    struct StructField {
        std::string name;
        TypePtr type;
    };

    struct StructDecl : Decl {
        std::string name;
        std::vector<StructField> fields;

        StructDecl() : Decl(DeclKind::Struct) {}
        std::string toString(int indent = 0) const override;
    };

    struct EnumVariant {
        std::string name;
        std::vector<TypePtr> payload;
    };

    struct EnumDecl : Decl {
        std::string name;
        std::vector<EnumVariant> variants;

        EnumDecl() : Decl(DeclKind::Enum) {}
        std::string toString(int indent = 0) const override;
    };

    struct ImplDecl : Decl {
        std::string typeName;
        std::vector<std::unique_ptr<FunctionDecl>> methods;

        ImplDecl() : Decl(DeclKind::Impl) {}
        std::string toString(int indent = 0) const override;
    };

    struct UseDecl : Decl {
        std::vector<std::string> path;
        UseDecl() : Decl(DeclKind::Use) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // ★ ImportDecl — import "file.mayami";
    // ============================================================
    struct ImportDecl : Decl {
        std::string path;   // "math_lib.mayami" или "lib/helpers.mayami"

        ImportDecl() : Decl(DeclKind::Import) {}
        std::string toString(int indent = 0) const override;
    };

    // ============================================================
    // Program
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