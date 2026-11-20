// src/parser/AST.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sstream>

#ifdef MAYAMI_CORE_EXPORTS
#define MAYAMI_API __declspec(dllexport)
#else
#define MAYAMI_API __declspec(dllimport)
#endif

namespace mayami {

    // ============================================================
    // Forward declarations
    // ============================================================
    struct MAYAMI_API Expr;
    struct MAYAMI_API Stmt;
    struct MAYAMI_API Decl;

    using ExprPtr = std::unique_ptr<Expr>;
    using StmtPtr = std::unique_ptr<Stmt>;
    using DeclPtr = std::unique_ptr<Decl>;

    // ============================================================
    // Типы (для AST)
    // ============================================================
    enum class MAYAMI_API TypeKind {
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

    struct MAYAMI_API Type {
        TypeKind kind = TypeKind::Unknown;
        std::string name;
        std::shared_ptr<Type> inner;
        size_t arraySize = 0;

        Type() = default;
        Type(const Type&) = delete;
        Type& operator=(const Type&) = delete;
        Type(Type&&) = default;
        Type& operator=(Type&&) = default;

        std::string toString() const;
    };

    using TypePtr = std::shared_ptr<Type>;

    // ============================================================
    // Expr
    // ============================================================
    enum class MAYAMI_API ExprKind {
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

    struct MAYAMI_API Expr {
        ExprKind kind;
        int line = 0;
        int column = 0;
        TypePtr type;

        Expr(ExprKind k) : kind(k) {}
        virtual ~Expr() = default;

        Expr(const Expr&) = delete;
        Expr& operator=(const Expr&) = delete;
        Expr(Expr&&) = default;
        Expr& operator=(Expr&&) = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct MAYAMI_API IntLitExpr : Expr {
        long long value;
        IntLitExpr() : Expr(ExprKind::IntLit) {}
        std::string toString(int indent = 0) const override;

        IntLitExpr(const IntLitExpr&) = delete;
        IntLitExpr& operator=(const IntLitExpr&) = delete;
        IntLitExpr(IntLitExpr&&) = default;
        IntLitExpr& operator=(IntLitExpr&&) = default;
    };

    struct MAYAMI_API FloatLitExpr : Expr {
        double value;
        FloatLitExpr() : Expr(ExprKind::FloatLit) {}
        std::string toString(int indent = 0) const override;

        FloatLitExpr(const FloatLitExpr&) = delete;
        FloatLitExpr& operator=(const FloatLitExpr&) = delete;
        FloatLitExpr(FloatLitExpr&&) = default;
        FloatLitExpr& operator=(FloatLitExpr&&) = default;
    };

    struct MAYAMI_API StringLitExpr : Expr {
        std::string value;
        StringLitExpr() : Expr(ExprKind::StringLit) {}
        std::string toString(int indent = 0) const override;

        StringLitExpr(const StringLitExpr&) = delete;
        StringLitExpr& operator=(const StringLitExpr&) = delete;
        StringLitExpr(StringLitExpr&&) = default;
        StringLitExpr& operator=(StringLitExpr&&) = default;
    };

    struct MAYAMI_API CharLitExpr : Expr {
        char value;
        CharLitExpr() : Expr(ExprKind::CharLit) {}
        std::string toString(int indent = 0) const override;

        CharLitExpr(const CharLitExpr&) = delete;
        CharLitExpr& operator=(const CharLitExpr&) = delete;
        CharLitExpr(CharLitExpr&&) = default;
        CharLitExpr& operator=(CharLitExpr&&) = default;
    };

    struct MAYAMI_API BoolLitExpr : Expr {
        bool value;
        BoolLitExpr() : Expr(ExprKind::BoolLit) {}
        std::string toString(int indent = 0) const override;

        BoolLitExpr(const BoolLitExpr&) = delete;
        BoolLitExpr& operator=(const BoolLitExpr&) = delete;
        BoolLitExpr(BoolLitExpr&&) = default;
        BoolLitExpr& operator=(BoolLitExpr&&) = default;
    };

    struct MAYAMI_API IdentifierExpr : Expr {
        std::string name;
        IdentifierExpr() : Expr(ExprKind::Identifier) {}
        std::string toString(int indent = 0) const override;

        IdentifierExpr(const IdentifierExpr&) = delete;
        IdentifierExpr& operator=(const IdentifierExpr&) = delete;
        IdentifierExpr(IdentifierExpr&&) = default;
        IdentifierExpr& operator=(IdentifierExpr&&) = default;
    };

    struct MAYAMI_API UnaryExpr : Expr {
        std::string op;
        ExprPtr operand;
        UnaryExpr() : Expr(ExprKind::Unary) {}
        std::string toString(int indent = 0) const override;

        UnaryExpr(const UnaryExpr&) = delete;
        UnaryExpr& operator=(const UnaryExpr&) = delete;
        UnaryExpr(UnaryExpr&&) = default;
        UnaryExpr& operator=(UnaryExpr&&) = default;
    };

    struct MAYAMI_API BinaryExpr : Expr {
        std::string op;
        ExprPtr left;
        ExprPtr right;
        BinaryExpr() : Expr(ExprKind::Binary) {}
        std::string toString(int indent = 0) const override;

        BinaryExpr(const BinaryExpr&) = delete;
        BinaryExpr& operator=(const BinaryExpr&) = delete;
        BinaryExpr(BinaryExpr&&) = default;
        BinaryExpr& operator=(BinaryExpr&&) = default;
    };

    struct MAYAMI_API AssignExpr : Expr {
        std::string op;
        ExprPtr target;
        ExprPtr value;
        AssignExpr() : Expr(ExprKind::Assign) {}
        std::string toString(int indent = 0) const override;

        AssignExpr(const AssignExpr&) = delete;
        AssignExpr& operator=(const AssignExpr&) = delete;
        AssignExpr(AssignExpr&&) = default;
        AssignExpr& operator=(AssignExpr&&) = default;
    };

    struct MAYAMI_API CallExpr : Expr {
        ExprPtr callee;
        std::vector<ExprPtr> args;
        CallExpr() : Expr(ExprKind::Call) {}
        std::string toString(int indent = 0) const override;

        CallExpr(const CallExpr&) = delete;
        CallExpr& operator=(const CallExpr&) = delete;
        CallExpr(CallExpr&&) = default;
        CallExpr& operator=(CallExpr&&) = default;
    };

    struct MAYAMI_API FieldExpr : Expr {
        ExprPtr object;
        std::string field;
        FieldExpr() : Expr(ExprKind::Field) {}
        std::string toString(int indent = 0) const override;

        FieldExpr(const FieldExpr&) = delete;
        FieldExpr& operator=(const FieldExpr&) = delete;
        FieldExpr(FieldExpr&&) = default;
        FieldExpr& operator=(FieldExpr&&) = default;
    };

    struct MAYAMI_API IndexExpr : Expr {
        ExprPtr object;
        ExprPtr index;
        IndexExpr() : Expr(ExprKind::Index) {}
        std::string toString(int indent = 0) const override;

        IndexExpr(const IndexExpr&) = delete;
        IndexExpr& operator=(const IndexExpr&) = delete;
        IndexExpr(IndexExpr&&) = default;
        IndexExpr& operator=(IndexExpr&&) = default;
    };

    struct MAYAMI_API GroupExpr : Expr {
        ExprPtr inner;
        GroupExpr() : Expr(ExprKind::Group) {}
        std::string toString(int indent = 0) const override;

        GroupExpr(const GroupExpr&) = delete;
        GroupExpr& operator=(const GroupExpr&) = delete;
        GroupExpr(GroupExpr&&) = default;
        GroupExpr& operator=(GroupExpr&&) = default;
    };

    struct MAYAMI_API ArrayLitExpr : Expr {
        std::vector<ExprPtr> elements;
        ArrayLitExpr() : Expr(ExprKind::ArrayLit) {}
        std::string toString(int indent = 0) const override;

        ArrayLitExpr(const ArrayLitExpr&) = delete;
        ArrayLitExpr& operator=(const ArrayLitExpr&) = delete;
        ArrayLitExpr(ArrayLitExpr&&) = default;
        ArrayLitExpr& operator=(ArrayLitExpr&&) = default;
    };

    struct MAYAMI_API StructLitField {
        std::string name;
        ExprPtr value;

        StructLitField() = default;
        StructLitField(const StructLitField&) = delete;
        StructLitField& operator=(const StructLitField&) = delete;
        StructLitField(StructLitField&&) = default;
        StructLitField& operator=(StructLitField&&) = default;
    };

    struct MAYAMI_API StructLitExpr : Expr {
        std::string typeName;
        std::vector<StructLitField> fields;
        StructLitExpr() : Expr(ExprKind::StructLit) {}
        std::string toString(int indent = 0) const override;

        StructLitExpr(const StructLitExpr&) = delete;
        StructLitExpr& operator=(const StructLitExpr&) = delete;
        StructLitExpr(StructLitExpr&&) = default;
        StructLitExpr& operator=(StructLitExpr&&) = default;
    };

    struct MAYAMI_API PathExpr : Expr {
        std::vector<std::string> segments;
        PathExpr() : Expr(ExprKind::Path) {}
        std::string toString(int indent = 0) const override;

        PathExpr(const PathExpr&) = delete;
        PathExpr& operator=(const PathExpr&) = delete;
        PathExpr(PathExpr&&) = default;
        PathExpr& operator=(PathExpr&&) = default;
    };

    // ============================================================
    // Stmt
    // ============================================================
    enum class MAYAMI_API StmtKind {
        Let, ExprStmt, Block, If, While, For, Loop,
        Return, Break, Continue,
    };

    struct MAYAMI_API Stmt {
        StmtKind kind;
        int line = 0;
        int column = 0;

        Stmt(StmtKind k) : kind(k) {}
        virtual ~Stmt() = default;

        Stmt(const Stmt&) = delete;
        Stmt& operator=(const Stmt&) = delete;
        Stmt(Stmt&&) = default;
        Stmt& operator=(Stmt&&) = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct MAYAMI_API LetStmt : Stmt {
        std::string name;
        bool isMut = false;
        TypePtr declaredType;
        ExprPtr init;

        LetStmt() : Stmt(StmtKind::Let) {}
        std::string toString(int indent = 0) const override;

        LetStmt(const LetStmt&) = delete;
        LetStmt& operator=(const LetStmt&) = delete;
        LetStmt(LetStmt&&) = default;
        LetStmt& operator=(LetStmt&&) = default;
    };

    struct MAYAMI_API ExprStmt : Stmt {
        ExprPtr expr;
        ExprStmt() : Stmt(StmtKind::ExprStmt) {}
        std::string toString(int indent = 0) const override;

        ExprStmt(const ExprStmt&) = delete;
        ExprStmt& operator=(const ExprStmt&) = delete;
        ExprStmt(ExprStmt&&) = default;
        ExprStmt& operator=(ExprStmt&&) = default;
    };

    struct MAYAMI_API BlockStmt : Stmt {
        std::vector<StmtPtr> statements;
        BlockStmt() : Stmt(StmtKind::Block) {}
        std::string toString(int indent = 0) const override;

        BlockStmt(const BlockStmt&) = delete;
        BlockStmt& operator=(const BlockStmt&) = delete;
        BlockStmt(BlockStmt&&) = default;
        BlockStmt& operator=(BlockStmt&&) = default;
    };

    struct MAYAMI_API IfStmt : Stmt {
        ExprPtr condition;
        StmtPtr thenBranch;
        StmtPtr elseBranch;
        IfStmt() : Stmt(StmtKind::If) {}
        std::string toString(int indent = 0) const override;

        IfStmt(const IfStmt&) = delete;
        IfStmt& operator=(const IfStmt&) = delete;
        IfStmt(IfStmt&&) = default;
        IfStmt& operator=(IfStmt&&) = default;
    };

    struct MAYAMI_API WhileStmt : Stmt {
        ExprPtr condition;
        StmtPtr body;
        WhileStmt() : Stmt(StmtKind::While) {}
        std::string toString(int indent = 0) const override;

        WhileStmt(const WhileStmt&) = delete;
        WhileStmt& operator=(const WhileStmt&) = delete;
        WhileStmt(WhileStmt&&) = default;
        WhileStmt& operator=(WhileStmt&&) = default;
    };

    struct MAYAMI_API ForStmt : Stmt {
        std::string varName;
        ExprPtr iterable;
        StmtPtr body;
        ForStmt() : Stmt(StmtKind::For) {}
        std::string toString(int indent = 0) const override;

        ForStmt(const ForStmt&) = delete;
        ForStmt& operator=(const ForStmt&) = delete;
        ForStmt(ForStmt&&) = default;
        ForStmt& operator=(ForStmt&&) = default;
    };

    struct MAYAMI_API LoopStmt : Stmt {
        StmtPtr body;
        LoopStmt() : Stmt(StmtKind::Loop) {}
        std::string toString(int indent = 0) const override;

        LoopStmt(const LoopStmt&) = delete;
        LoopStmt& operator=(const LoopStmt&) = delete;
        LoopStmt(LoopStmt&&) = default;
        LoopStmt& operator=(LoopStmt&&) = default;
    };

    struct MAYAMI_API ReturnStmt : Stmt {
        ExprPtr value;
        ReturnStmt() : Stmt(StmtKind::Return) {}
        std::string toString(int indent = 0) const override;

        ReturnStmt(const ReturnStmt&) = delete;
        ReturnStmt& operator=(const ReturnStmt&) = delete;
        ReturnStmt(ReturnStmt&&) = default;
        ReturnStmt& operator=(ReturnStmt&&) = default;
    };

    struct MAYAMI_API BreakStmt : Stmt {
        BreakStmt() : Stmt(StmtKind::Break) {}
        std::string toString(int indent = 0) const override;

        BreakStmt(const BreakStmt&) = delete;
        BreakStmt& operator=(const BreakStmt&) = delete;
        BreakStmt(BreakStmt&&) = default;
        BreakStmt& operator=(BreakStmt&&) = default;
    };

    struct MAYAMI_API ContinueStmt : Stmt {
        ContinueStmt() : Stmt(StmtKind::Continue) {}
        std::string toString(int indent = 0) const override;

        ContinueStmt(const ContinueStmt&) = delete;
        ContinueStmt& operator=(const ContinueStmt&) = delete;
        ContinueStmt(ContinueStmt&&) = default;
        ContinueStmt& operator=(ContinueStmt&&) = default;
    };

    // ============================================================
    // Decl
    // ============================================================
    enum class MAYAMI_API DeclKind {
        Function,
        Struct,
        Enum,
        Impl,
        Use,
        Import,
    };

    struct MAYAMI_API Decl {
        DeclKind kind;
        int line = 0;
        int column = 0;

        Decl(DeclKind k) : kind(k) {}
        virtual ~Decl() = default;

        Decl(const Decl&) = delete;
        Decl& operator=(const Decl&) = delete;
        Decl(Decl&&) = default;
        Decl& operator=(Decl&&) = default;

        virtual std::string toString(int indent = 0) const = 0;
    };

    struct MAYAMI_API Param {
        std::string name;
        TypePtr type;

        Param() = default;
        Param(const Param&) = delete;
        Param& operator=(const Param&) = delete;
        Param(Param&&) = default;
        Param& operator=(Param&&) = default;
    };

    struct MAYAMI_API FunctionDecl : Decl {
        std::string name;
        std::vector<Param> params;
        TypePtr returnType;
        std::unique_ptr<BlockStmt> body;

        FunctionDecl() : Decl(DeclKind::Function) {}
        std::string toString(int indent = 0) const override;

        FunctionDecl(const FunctionDecl&) = delete;
        FunctionDecl& operator=(const FunctionDecl&) = delete;
        FunctionDecl(FunctionDecl&&) = default;
        FunctionDecl& operator=(FunctionDecl&&) = default;
    };

    struct MAYAMI_API StructField {
        std::string name;
        TypePtr type;

        StructField() = default;
        StructField(const StructField&) = delete;
        StructField& operator=(const StructField&) = delete;
        StructField(StructField&&) = default;
        StructField& operator=(StructField&&) = default;
    };

    struct MAYAMI_API StructDecl : Decl {
        std::string name;
        std::vector<StructField> fields;

        StructDecl() : Decl(DeclKind::Struct) {}
        std::string toString(int indent = 0) const override;

        StructDecl(const StructDecl&) = delete;
        StructDecl& operator=(const StructDecl&) = delete;
        StructDecl(StructDecl&&) = default;
        StructDecl& operator=(StructDecl&&) = default;
    };

    struct MAYAMI_API EnumVariant {
        std::string name;
        std::vector<TypePtr> payload;

        EnumVariant() = default;
        EnumVariant(const EnumVariant&) = delete;
        EnumVariant& operator=(const EnumVariant&) = delete;
        EnumVariant(EnumVariant&&) = default;
        EnumVariant& operator=(EnumVariant&&) = default;
    };

    struct MAYAMI_API EnumDecl : Decl {
        std::string name;
        std::vector<EnumVariant> variants;

        EnumDecl() : Decl(DeclKind::Enum) {}
        std::string toString(int indent = 0) const override;

        EnumDecl(const EnumDecl&) = delete;
        EnumDecl& operator=(const EnumDecl&) = delete;
        EnumDecl(EnumDecl&&) = default;
        EnumDecl& operator=(EnumDecl&&) = default;
    };

    struct MAYAMI_API ImplDecl : Decl {
        std::string typeName;
        std::vector<std::unique_ptr<FunctionDecl>> methods;

        ImplDecl() : Decl(DeclKind::Impl) {}
        std::string toString(int indent = 0) const override;

        ImplDecl(const ImplDecl&) = delete;
        ImplDecl& operator=(const ImplDecl&) = delete;
        ImplDecl(ImplDecl&&) = default;
        ImplDecl& operator=(ImplDecl&&) = default;
    };

    struct MAYAMI_API UseDecl : Decl {
        std::vector<std::string> path;

        UseDecl() : Decl(DeclKind::Use) {}
        std::string toString(int indent = 0) const override;

        UseDecl(const UseDecl&) = delete;
        UseDecl& operator=(const UseDecl&) = delete;
        UseDecl(UseDecl&&) = default;
        UseDecl& operator=(UseDecl&&) = default;
    };

    struct MAYAMI_API ImportDecl : Decl {
        std::string path;

        ImportDecl() : Decl(DeclKind::Import) {}
        std::string toString(int indent = 0) const override;

        ImportDecl(const ImportDecl&) = delete;
        ImportDecl& operator=(const ImportDecl&) = delete;
        ImportDecl(ImportDecl&&) = default;
        ImportDecl& operator=(ImportDecl&&) = default;
    };

    // ============================================================
    // Program
    // ============================================================
    struct MAYAMI_API Program {
        std::vector<DeclPtr> declarations;
        std::string toString() const;

        Program() = default;
        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;
        Program(Program&&) = default;
        Program& operator=(Program&&) = default;
    };

    // ============================================================
    // Утилиты
    // ============================================================
    MAYAMI_API std::string indentStr(int n);
    MAYAMI_API std::string typeKindName(TypeKind kind);

} // namespace mayami