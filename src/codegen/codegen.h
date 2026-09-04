#pragma once

#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <cstdio>

// LLVM Includes
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Constants.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/DynamicLibrary.h>

#include "../parser/ast.h"
#include "environment.h"

// dllexport is only meaningful (and only legal syntax) on Windows, where the
// JIT engine needs these symbols exported from the compiler's own binary so
// MCJIT's runtime symbol resolution can find them. On Linux/macOS, symbols
// with external linkage are visible to MCJIT's process-symbol lookup without
// any extra attribute, so JIT_API reduces to a plain `extern "C"` there.
#if defined(_WIN32)
#define JIT_API extern "C" __declspec(dllexport)
#else
#define JIT_API extern "C"
#endif

// --- Native Function Signatures Only ---
JIT_API double nova_init_window(double width, double height, const char* title);
JIT_API double nova_draw_rect(double x, double y, double w, double h);
// Same as nova_draw_rect but with an explicit 0-255 RGB color, so scripts
// can tell the snake, food, walls, and UI panels apart instead of every
// rectangle coming out white.
JIT_API double nova_draw_rect_color(double x, double y, double w, double h, double r, double g, double b);
// Renders a Nova string's characters as on-screen text at (x, y) with the
// given pixel size. `strBuf` is a raw Nova heap pointer (tag/length/chars —
// see the layout comment above nova_print_string in codegen.cpp), decoded
// the same way nova_print_string decodes it, just handed to raylib's
// DrawText instead of printf.
JIT_API double nova_draw_text(double x, double y, double size, double* strBuf);
// Draws `value` as on-screen text (formatted as an integer) at (x, y) —
// exists so scripts can show a score/counter without needing a
// number-to-string conversion in the language itself.
JIT_API double nova_draw_number(double x, double y, double size, double value);
JIT_API double nova_clear_screen();
JIT_API double nova_render_frame();
JIT_API double nova_is_key_down(double keycode);
JIT_API double nova_random(double minVal, double maxVal);
JIT_API double nova_close_window();
extern "C" void nova_bounds_error(double idx, double len);
JIT_API double nova_set_fps(double fps);
extern "C" void nova_print(double val);

// --- CodeGen Class Structure Only ---
// CodeGen implements StmtVisitor/ExprVisitor (declared in ast.h) so that each
// AST node resolves its own codegen routine via accept() at compile-time,
// instead of a dynamic_cast chain probing node types at runtime.
class CodeGen : public StmtVisitor, public ExprVisitor {
public:
    // `wasmTarget`: pass true when this module will end up compiled for
    // wasm32 (e.g. via emcc), rather than JIT-executed natively by
    // execute(). wasm32's malloc/calloc take 32-bit size_t, unlike the
    // 64-bit size_t on every desktop target this class originally assumed
    // — see the wasmTarget-gated code in getMallocFn/getCallocFn and every
    // call site that builds a malloc/calloc size argument (visitArrayLiteralExpr,
    // the string branch of visitLiteralExpr, and the array_new builtin in
    // visitCallExpr). Declaring those with the wrong integer width doesn't
    // fail to compile the .nv script itself — it fails much later, as a
    // wasm-ld "function signature mismatch" when the resulting bitcode is
    // linked against the real (32-bit) libc malloc/calloc.
    CodeGen(const std::string& moduleName, bool wasmTarget = false);
    void generate(const std::vector<StmtPtr>& statements);
    void execute();
    // Raw, non-owning access to the generated module — CodeGen keeps
    // ownership (via the unique_ptr below), so callers must not outlive
    // this CodeGen instance or try to delete/take ownership of the
    // pointer. Exists for drivers that need to hand the module to
    // something CodeGen itself doesn't do, like WriteBitcodeToFile for
    // the wasm/--emit-bc path in main.cpp.
    llvm::Module* getModule() const { return module.get(); }

private:
    bool wasmTarget = false;
    // The integer type malloc/calloc's size_t arguments should use: i32 for
    // wasm32, i64 for every native desktop target execute() JITs on.
    llvm::IntegerType* sizeTy() const;

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;
    llvm::AllocaInst* returnAlloc = nullptr;
    std::shared_ptr<Environment> env;

    // Innermost-active-loop's exit/condition blocks, pushed in
    // visitWhileStmt and popped when that loop's body finishes generating.
    // `break` branches to breakTargets.back(), `continue` to
    // continueTargets.back() — a stack rather than a single slot so nested
    // `grind` loops each resolve to their own loop, not the outermost one.
    std::vector<llvm::BasicBlock*> breakTargets;
    std::vector<llvm::BasicBlock*> continueTargets;

    // Thin dispatch entry points — kept so call sites elsewhere don't change.
    // They just forward to stmt->accept(*this) / expr->accept(*this).
    void generateStatement(Stmt* stmt);
    llvm::Value* generateExpression(Expression* expr);

    // --- StmtVisitor overrides: one method per node type, no casting ---
    void visitLetStmt(LetStmt* stmt) override;
    void visitExpressionStmt(ExpressionStmt* stmt) override;
    void visitBlockStmt(BlockStmt* stmt) override;
    void visitIfStmt(IfStmt* stmt) override;
    void visitWhileStmt(WhileStmt* stmt) override;
    void visitFunctionStmt(FunctionStmt* stmt) override;
    void visitReturnStmt(ReturnStmt* stmt) override;
    void visitBreakStmt(BreakStmt* stmt) override;
    void visitContinueStmt(ContinueStmt* stmt) override;

    // --- ExprVisitor overrides ---
    llvm::Value* visitLiteralExpr(LiteralExpression* expr) override;
    llvm::Value* visitVariableExpr(VariableExpression* expr) override;
    llvm::Value* visitBinaryExpr(BinaryExpression* expr) override;
    llvm::Value* visitUnaryExpr(UnaryExpression* expr) override;
    llvm::Value* visitGroupingExpr(GroupingExpression* expr) override;
    llvm::Value* visitAssignmentExpr(AssignmentExpression* expr) override;
    llvm::Value* visitLogicalExpr(LogicalExpression* expr) override;
    llvm::Value* visitCallExpr(CallExpression* expr) override;
    llvm::Value* visitArrayLiteralExpr(ArrayLiteralExpression* expr) override;
    llvm::Value* visitIndexExpr(IndexExpression* expr) override;
    llvm::Value* visitIndexAssignExpr(IndexAssignmentExpression* expr) override;
};