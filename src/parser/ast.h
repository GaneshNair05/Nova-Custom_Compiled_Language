#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../lexer/lexer.h"
inline void printIndent(int indent) {
    for (int i = 0; i < indent; ++i) std::cout << "  ";
}

namespace llvm { class Value; }

// Forward declare all nodes
struct LiteralExpression; struct VariableExpression; struct BinaryExpression;
struct UnaryExpression; struct GroupingExpression; struct AssignmentExpression;
struct LogicalExpression; struct CallExpression;
struct ArrayLiteralExpression; struct IndexExpression; struct IndexAssignmentExpression;
struct LetStmt; struct ExpressionStmt; struct BlockStmt; struct IfStmt;
struct WhileStmt; struct FunctionStmt; struct ReturnStmt;
struct BreakStmt; struct ContinueStmt;

// Define the Visitor Interfaces
struct ExprVisitor {
    virtual llvm::Value* visitLiteralExpr(LiteralExpression* expr) = 0;
    virtual llvm::Value* visitVariableExpr(VariableExpression* expr) = 0;
    virtual llvm::Value* visitBinaryExpr(BinaryExpression* expr) = 0;
    virtual llvm::Value* visitUnaryExpr(UnaryExpression* expr) = 0;
    virtual llvm::Value* visitGroupingExpr(GroupingExpression* expr) = 0;
    virtual llvm::Value* visitAssignmentExpr(AssignmentExpression* expr) = 0;
    virtual llvm::Value* visitLogicalExpr(LogicalExpression* expr) = 0;
    virtual llvm::Value* visitCallExpr(CallExpression* expr) = 0;
    virtual llvm::Value* visitArrayLiteralExpr(ArrayLiteralExpression* expr) = 0;
    virtual llvm::Value* visitIndexExpr(IndexExpression* expr) = 0;
    virtual llvm::Value* visitIndexAssignExpr(IndexAssignmentExpression* expr) = 0;
};

struct StmtVisitor {
    virtual void visitLetStmt(LetStmt* stmt) = 0;
    virtual void visitExpressionStmt(ExpressionStmt* stmt) = 0;
    virtual void visitBlockStmt(BlockStmt* stmt) = 0;
    virtual void visitIfStmt(IfStmt* stmt) = 0;
    virtual void visitWhileStmt(WhileStmt* stmt) = 0;
    virtual void visitFunctionStmt(FunctionStmt* stmt) = 0;
    virtual void visitReturnStmt(ReturnStmt* stmt) = 0;
    virtual void visitBreakStmt(BreakStmt* stmt) = 0;
    virtual void visitContinueStmt(ContinueStmt* stmt) = 0;
};
//Operators
struct Expression {
    virtual ~Expression() = default;
    virtual void print(int indent = 0) const = 0;
    virtual llvm::Value* accept(ExprVisitor& visitor) = 0; // NEW
};


using ExpressionPtr = std::unique_ptr<Expression>;
struct LiteralExpression : Expression{
    Token value;
    explicit LiteralExpression(Token value) : value(std::move(value)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "LiteralExpression(" << value.value << ")\n";
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitLiteralExpr(this); }
};
struct VariableExpression : Expression{
    std::string name;
    explicit VariableExpression(std::string name) : name(std::move(name)){}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "VariableExpression(" << name << ")\n";
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitVariableExpr(this); }
};

struct BinaryExpression : Expression{
    ExpressionPtr left;
    Token op;
    ExpressionPtr right;
    BinaryExpression(ExpressionPtr left, Token op, ExpressionPtr right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)){}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "BinaryExpression(" << op.value << ")\n";
        left->print(indent + 1);
        right->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitBinaryExpr(this); }
};  

struct UnaryExpression : Expression{
    Token op;
    ExpressionPtr right;
    UnaryExpression(Token op, ExpressionPtr right)
        :op(std::move(op)), right(std::move(right)){}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "UnaryExpression(" << op.value << ")\n";
        right->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitUnaryExpr(this); }
};
struct GroupingExpression : Expression{
    ExpressionPtr inner;
    explicit GroupingExpression(ExpressionPtr inner) : inner(std::move(inner)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "GroupingExpression:\n";
        inner->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitGroupingExpr(this); }
};
struct AssignmentExpression : Expression {
    std::string name;
    ExpressionPtr value;
    AssignmentExpression(std::string name, ExpressionPtr value)
        : name(std::move(name)), value(std::move(value)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "AssignmentExpression(" << name << " =)\n";
        value->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitAssignmentExpr(this); }
};
struct LogicalExpression : Expression {
    ExpressionPtr left;
    Token op;
    ExpressionPtr right;
    LogicalExpression(ExpressionPtr left, Token op, ExpressionPtr right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "LogicalExpression(" << op.value << ")\n";
        left->print(indent + 1);
        right->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitLogicalExpr(this); }
};
struct CallExpression : Expression {
    ExpressionPtr callee;
    Token paren;
    std::vector<ExpressionPtr> args;
    CallExpression(ExpressionPtr callee, Token paren, std::vector<ExpressionPtr> args)
        : callee(std::move(callee)), paren(std::move(paren)), args(std::move(args)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "CallExpression:\n";
        printIndent(indent + 1);
        std::cout << "Callee:\n";
        callee->print(indent + 2);
        printIndent(indent + 1);
        std::cout << "Arguments:\n";
        for (const auto& arg : args) {
            arg->print(indent + 2);
        }
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitCallExpr(this); }
};
// [e1, e2, ...] — evaluates to a heap pointer (see CodeGen::visitArrayLiteralExpr),
// not a double, so it's a distinct value kind from every other expression here.
struct ArrayLiteralExpression : Expression {
    std::vector<ExpressionPtr> elements;
    explicit ArrayLiteralExpression(std::vector<ExpressionPtr> elements)
        : elements(std::move(elements)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "ArrayLiteralExpression:\n";
        for (const auto& el : elements) {
            el->print(indent + 1);
        }
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitArrayLiteralExpr(this); }
};

// arr[index] — read. `array` is any expression that evaluates to a heap
// pointer (a variable, a call to array_new(), a literal, etc).
struct IndexExpression : Expression {
    ExpressionPtr array;
    Token bracket; // the '[' token, kept for error reporting like CallExpression::paren
    ExpressionPtr index;
    IndexExpression(ExpressionPtr array, Token bracket, ExpressionPtr index)
        : array(std::move(array)), bracket(std::move(bracket)), index(std::move(index)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "IndexExpression:\n";
        array->print(indent + 1);
        index->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitIndexExpr(this); }
};

// arr[index] = value — write. Produced by Parser::assignment() when it sees
// an IndexExpression on the left of '=', mirroring how AssignmentExpression
// is produced for a bare VariableExpression.
struct IndexAssignmentExpression : Expression {
    ExpressionPtr array;
    Token bracket;
    ExpressionPtr index;
    ExpressionPtr value;
    IndexAssignmentExpression(ExpressionPtr array, Token bracket, ExpressionPtr index, ExpressionPtr value)
        : array(std::move(array)), bracket(std::move(bracket)), index(std::move(index)), value(std::move(value)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "IndexAssignmentExpression:\n";
        array->print(indent + 1);
        index->print(indent + 1);
        value->print(indent + 1);
    }
    llvm::Value* accept(ExprVisitor& visitor) override { return visitor.visitIndexAssignExpr(this); }
};

// Statements
struct Stmt {
    virtual ~Stmt() = default;
    virtual void print(int indent = 0) const = 0;
    virtual void accept(StmtVisitor& visitor) = 0; // NEW
};
struct LetStmt : Stmt{
    std::string name;
    ExpressionPtr initializer;
    explicit LetStmt(std::string name, ExpressionPtr initializer) 
    : name(std::move(name)) , initializer(std::move(initializer)){}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "LetStmt(" << name << ")\n";
        if (initializer) {
            initializer->print(indent + 1);
        }
    }
    void accept(StmtVisitor& visitor) override { visitor.visitLetStmt(this); }
};
using StmtPtr = std::unique_ptr<Stmt>;
struct ExpressionStmt : Stmt {
    ExpressionPtr expr;
    explicit ExpressionStmt(ExpressionPtr expr) : expr(std::move(expr)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "ExpressionStmt:\n";
        expr->print(indent + 1);
    }
    void accept(StmtVisitor& visitor) override { visitor.visitExpressionStmt(this); }
};
struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
    explicit BlockStmt(std::vector<StmtPtr> statements) : statements(std::move(statements)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "BlockStmt:\n";
        for (const auto& stmt : statements) {
            if (stmt) stmt->print(indent + 1);
        }
    }
    void accept(StmtVisitor& visitor) override { visitor.visitBlockStmt(this); }
};
struct IfStmt : Stmt {
    ExpressionPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch; // nullable
    IfStmt(ExpressionPtr c, StmtPtr t, StmtPtr e)
        : condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "IfStmt:\n";
        printIndent(indent + 1); std::cout << "Condition:\n";
        condition->print(indent + 2);
        printIndent(indent + 1); std::cout << "Then:\n";
        thenBranch->print(indent + 2);
        if (elseBranch) {
            printIndent(indent + 1); std::cout << "Else:\n";
            elseBranch->print(indent + 2);
        }
    }
    void accept(StmtVisitor& visitor) override { visitor.visitIfStmt(this); }
};
struct WhileStmt : Stmt {
    ExpressionPtr condition;
    StmtPtr body;
    WhileStmt(ExpressionPtr c, StmtPtr b) : condition(std::move(c)), body(std::move(b)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "WhileStmt:\n";
        printIndent(indent + 1); std::cout << "Condition:\n";
        condition->print(indent + 2);
        printIndent(indent + 1); std::cout << "Body:\n";
        body->print(indent + 2);
    }
    void accept(StmtVisitor& visitor) override { visitor.visitWhileStmt(this); }
};
struct FunctionStmt : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    FunctionStmt(std::string n, std::vector<std::string> p, std::vector<StmtPtr> b)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "FunctionStmt(" << name << ")\n";
        printIndent(indent + 1); std::cout << "Params: ";
        for (const auto& p : params) std::cout << p << " ";
        std::cout << "\n";
        printIndent(indent + 1); std::cout << "Body:\n";
        for (const auto& stmt : body) {
            if (stmt) stmt->print(indent + 2);
        }
    }
    void accept(StmtVisitor& visitor) override { visitor.visitFunctionStmt(this); }
};
struct ReturnStmt : Stmt {
    Token keyword;
    ExpressionPtr value; // nullable
    ReturnStmt(Token k, ExpressionPtr v) : keyword(std::move(k)), value(std::move(v)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "ReturnStmt\n";
        if (value) {
            value->print(indent + 1);
        }
    }
    void accept(StmtVisitor& visitor) override { visitor.visitReturnStmt(this); }
};
struct BreakStmt : Stmt {
    Token keyword; // the 'break' token, kept for error reporting (e.g. break used outside a loop)
    explicit BreakStmt(Token keyword) : keyword(std::move(keyword)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "BreakStmt\n";
    }
    void accept(StmtVisitor& visitor) override { visitor.visitBreakStmt(this); }
};
struct ContinueStmt : Stmt {
    Token keyword; // the 'continue' token, kept for error reporting
    explicit ContinueStmt(Token keyword) : keyword(std::move(keyword)) {}
    void print(int indent = 0) const override {
        printIndent(indent);
        std::cout << "ContinueStmt\n";
    }
    void accept(StmtVisitor& visitor) override { visitor.visitContinueStmt(this); }
};