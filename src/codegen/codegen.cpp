#include "codegen.h"
#include <raylib.h>
#include <ctime>

// Force Windows to expose these functions to the JIT engine. On other
// platforms JIT_API is already defined (as a plain extern "C") by
// codegen.h, included just above — don't redefine it here.
#if defined(_WIN32)
#define JIT_API extern "C" __declspec(dllexport)
#endif

JIT_API double nova_init_window(double width, double height, const char* title) {
    InitWindow((int)width, (int)height, title ? title : "Nova Engine");
    return 0.0;
}

JIT_API double nova_set_fps(double fps) {
    SetTargetFPS((int)fps);
    return 0.0;
}

JIT_API double nova_draw_rect(double x, double y, double w, double h) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, WHITE);
    return 0.0;
}

JIT_API double nova_draw_rect_color(double x, double y, double w, double h, double r, double g, double b) {
    auto clamp255 = [](double v) -> unsigned char {
        if (v < 0.0) v = 0.0;
        if (v > 255.0) v = 255.0;
        return (unsigned char)v;
    };
    Color c = { clamp255(r), clamp255(g), clamp255(b), 255 };
    DrawRectangle((int)x, (int)y, (int)w, (int)h, c);
    return 0.0;
}

JIT_API double nova_draw_text(double x, double y, double size, double* strBuf) {
    if (!strBuf) return 0.0;
    // Same decode loop as nova_print_string below: slot 1 is the character
    // count, slots 2.. are code points stored one per double.
    double lenD = strBuf[1];
    size_t len = lenD > 0 ? (size_t)lenD : 0;
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        s += (char)(int)strBuf[2 + i];
    }
    DrawText(s.c_str(), (int)x, (int)y, (int)size, WHITE);
    return 0.0;
}

JIT_API double nova_draw_number(double x, double y, double size, double value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f", value);
    DrawText(buf, (int)x, (int)y, (int)size, WHITE);
    return 0.0;
}

JIT_API double nova_clear_screen() {
    BeginDrawing();
    ClearBackground(BLACK);
    return 0.0;
}

JIT_API double nova_render_frame() {
    EndDrawing();
    return 0.0;
}

JIT_API double nova_is_key_down(double keycode) {
    return IsKeyDown((int)keycode) ? 1.0 : 0.0;
}

JIT_API double nova_random(double minVal, double maxVal) {
    // GetRandomValue is inclusive on both ends. Seeded once, lazily, on
    // first call — was previously unseeded and deterministic run-to-run.
    static bool seeded = false;
    if (!seeded) {
        SetRandomSeed((unsigned int)time(nullptr));
        seeded = true;
    }
    return (double)GetRandomValue((int)minVal, (int)maxVal);
}

JIT_API double nova_close_window() {
    CloseWindow();
    return 0.0;
}

extern "C" void nova_bounds_error(double idx, double len) {
    std::cerr << "Runtime Error: array index " << idx << " out of bounds (length " << len << ")\n";
}

//Physical C++ fn that the language triggers.
extern "C" void nova_print(double val) {
    printf(">>> %f\n",val);
    fflush(stdout);
}

// --- Heap allocation helpers for arrays AND strings ---
// Arrays and strings share one physical heap layout, all slots doubles:
//   slot 0: tag    — NOVA_TAG_ARRAY or NOVA_TAG_STRING (see below)
//   slot 1: length — element count (array) or character count (string)
//   slot 2..: data — array elements, or a string's characters stored one
//                    per slot as that character's code point (so a string
//                    is, physically, just an array of char codes)
// Sharing the layout means indexing, length(), and free_array() all work
// on strings automatically once their offsets below account for the header
// being 2 slots instead of 1 (see NOVA_HEADER_SLOTS) — the only place that
// genuinely needs to tell the two apart is wherever behavior actually
// differs by kind (announce()'s print routing, +/==  on two pointers), and
// that's a runtime check against slot 0, not something the compiler can
// know in a language with no static type system. This is what makes
// runtime bounds checking possible (see visitIndexExpr/visitIndexAssignExpr)
// and length() free to add for strings too. We don't need a custom nova_*
// wrapper the way raylib calls need one: malloc/calloc are already linked
// into this process, and MCJIT resolves unmapped external symbols against
// the host process (see DynamicLibrary::LoadLibraryPermanently in
// execute()), so declaring them here is enough to call them from JIT'd IR.
static constexpr double NOVA_TAG_ARRAY = 0.0;
static constexpr double NOVA_TAG_STRING = 1.0;
static constexpr uint64_t NOVA_HEADER_SLOTS = 2;

// Prints a Nova string's actual characters (not its heap address). Reads
// slot 1 for the character count, slots 2.. for the code points.
extern "C" void nova_print_string(double* buf) {
    double lenD = buf[1];
    size_t len = lenD > 0 ? (size_t)lenD : 0;
    std::string s;
    s.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        s += (char)(int)buf[NOVA_HEADER_SLOTS + i];
    }
    printf(">>> %s\n", s.c_str());
    fflush(stdout);
}

// Prints a Nova array as "[e1, e2, ...]". This is what announce() falls
// back to for any pointer value whose tag isn't NOVA_TAG_STRING.
extern "C" void nova_print_array(double* buf) {
    double lenD = buf[1];
    size_t len = lenD > 0 ? (size_t)lenD : 0;
    printf(">>> [");
    for (size_t i = 0; i < len; ++i) {
        printf("%g", buf[NOVA_HEADER_SLOTS + i]);
        if (i + 1 < len) printf(", ");
    }
    printf("]\n");
    fflush(stdout);
}

// `a + b` where both are Nova strings: allocates a new heap string of
// combined length and copies both characters in. Doesn't itself verify
// both inputs are actually tagged NOVA_TAG_STRING — visitBinaryExpr only
// calls this once both operands are pointer-typed, not once it's confirmed
// which *kind* of pointer (see the caveat on the Plus case there).
extern "C" double* nova_string_concat(double* a, double* b) {
    size_t lenA = a[1] > 0 ? (size_t)a[1] : 0;
    size_t lenB = b[1] > 0 ? (size_t)b[1] : 0;
    size_t totalLen = lenA + lenB;
    double* result = (double*)malloc((totalLen + NOVA_HEADER_SLOTS) * sizeof(double));
    result[0] = NOVA_TAG_STRING;
    result[1] = (double)totalLen;
    for (size_t i = 0; i < lenA; ++i) result[NOVA_HEADER_SLOTS + i] = a[NOVA_HEADER_SLOTS + i];
    for (size_t i = 0; i < lenB; ++i) result[NOVA_HEADER_SLOTS + lenA + i] = b[NOVA_HEADER_SLOTS + i];
    return result;
}

// Character-by-character content equality for two Nova strings (`==`/`!=`
// desugars to this then, for !=, negates the result — see
// visitBinaryExpr). Returns 0.0 if either side isn't actually tagged as a
// string, so comparing a string to an array reports "not equal" instead of
// reading the array's numbers as if they were character codes.
extern "C" double nova_string_equals(double* a, double* b) {
    if (a[0] != NOVA_TAG_STRING || b[0] != NOVA_TAG_STRING) return 0.0;
    if (a[1] != b[1]) return 0.0;
    size_t len = a[1] > 0 ? (size_t)a[1] : 0;
    for (size_t i = 0; i < len; ++i) {
        if (a[NOVA_HEADER_SLOTS + i] != b[NOVA_HEADER_SLOTS + i]) return 0.0;
    }
    return 1.0;
}
// `sizeTy` must match the real malloc/calloc size_t width on whatever
// target actually links this IR: i64 on every desktop target (the
// original assumption here), but i32 on wasm32 — declaring these with the
// wrong width doesn't break compiling the .nv script, it breaks linking
// the resulting bitcode against the real libc malloc/calloc later ("function
// signature mismatch" from wasm-ld). See CodeGen::sizeTy().
static llvm::FunctionCallee getMallocFn(llvm::Module* module, llvm::IRBuilder<>& builder, llvm::IntegerType* sizeTy) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getPtrTy(), {sizeTy}, false);
    return module->getOrInsertFunction("malloc", ft);
}
static llvm::FunctionCallee getCallocFn(llvm::Module* module, llvm::IRBuilder<>& builder, llvm::IntegerType* sizeTy) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getPtrTy(), {sizeTy, sizeTy}, false);
    return module->getOrInsertFunction("calloc", ft);
}
// Pairs with getMallocFn/getCallocFn above: arrays are never freed
// automatically (this language has no GC/refcounting), so a script that
// allocates arrays in a loop — e.g. once per frame in a `grind` — leaks
// until free_array() is called explicitly. See the free_array builtin in
// visitCallExpr.
static llvm::FunctionCallee getFreeFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false);
    return module->getOrInsertFunction("free", ft);
}
static llvm::FunctionCallee getBoundsErrorFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getVoidTy(), {builder.getDoubleTy(), builder.getDoubleTy()}, false);
    return module->getOrInsertFunction("nova_bounds_error", ft);
}
static llvm::FunctionCallee getPrintStringFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false);
    return module->getOrInsertFunction("nova_print_string", ft);
}
static llvm::FunctionCallee getPrintArrayFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getVoidTy(), {builder.getPtrTy()}, false);
    return module->getOrInsertFunction("nova_print_array", ft);
}
static llvm::FunctionCallee getStringConcatFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getPtrTy(), {builder.getPtrTy(), builder.getPtrTy()}, false);
    return module->getOrInsertFunction("nova_string_concat", ft);
}
static llvm::FunctionCallee getStringEqualsFn(llvm::Module* module, llvm::IRBuilder<>& builder) {
    llvm::FunctionType* ft = llvm::FunctionType::get(builder.getDoubleTy(), {builder.getPtrTy(), builder.getPtrTy()}, false);
    return module->getOrInsertFunction("nova_string_equals", ft);
}
static constexpr uint64_t NOVA_ELEM_SIZE = 8; // sizeof(double)

// AllocaInst and GlobalVariable both represent "an address holding a value
// of some type" but expose it via different accessors (getAllocatedType()
// vs getValueType()) — this normalizes the two so visitVariableExpr etc.
// don't need to care which kind of slot a given name resolved to.
static llvm::Type* getSlotType(llvm::Value* slot) {
    if (auto* AI = llvm::dyn_cast<llvm::AllocaInst>(slot)) return AI->getAllocatedType();
    if (auto* GV = llvm::dyn_cast<llvm::GlobalVariable>(slot)) return GV->getValueType();
    return nullptr;
}

CodeGen::CodeGen(const std::string& moduleName, bool wasmTarget)
    : wasmTarget(wasmTarget) {
    context = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>(moduleName, *context);
    builder = std::make_unique<llvm::IRBuilder<>>(*context);
    env = std::make_shared<Environment>();

    llvm::FunctionType* printType = llvm::FunctionType::get(
        builder->getVoidTy(),
        {llvm::Type::getDoubleTy(*context)},
        false
    );
    llvm::Function::Create(printType, llvm::Function::ExternalLinkage, "print", module.get());
}

llvm::IntegerType* CodeGen::sizeTy() const {
    return wasmTarget ? builder->getInt32Ty() : builder->getInt64Ty();
}

void CodeGen::generate(const std::vector<StmtPtr>& statements) {
    // Named "nova_main", not "main": on the native JIT path this doesn't
    // matter, but on the wasm/emcc path a function literally named "main"
    // collides with the C runtime's own notion of the program entry point
    // (Emscripten's startup code wants to own that symbol) — that collision
    // is a likely source of the "duplicate export name" link error. Give
    // the wasm build's own C `main()` a body that just calls nova_main().
    llvm::FunctionType* funcType = llvm::FunctionType::get(llvm::Type::getDoubleTy(*context), false);
    llvm::Function* mainFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "nova_main", module.get());

    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*context, "entry", mainFunc);
    builder->SetInsertPoint(entryBlock);

    returnAlloc = builder->CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, "ret_val");
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)), returnAlloc);

    for (const auto& stmt : statements) {
        generateStatement(stmt.get());
        // A top-level `reward` (legal, if unusual, outside a skill) already
        // terminates this block; don't let later statements append more
        // instructions after it (invalid IR — see the matching guard in
        // visitBlockStmt below).
        if (builder->GetInsertBlock()->getTerminator()) break;
    }
    if (!builder->GetInsertBlock()->getTerminator()) {
        llvm::Value* finalVal = builder->CreateLoad(llvm::Type::getDoubleTy(*context), returnAlloc, "final_ret");
        builder->CreateRet(finalVal);
    }
    module->print(llvm::outs(), nullptr);
}
void CodeGen::execute() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    
    llvm::Module* modulePtr = module.get();
    std::string errStr;
    llvm::ExecutionEngine* engine = llvm::EngineBuilder(std::move(module)).setErrorStr(&errStr).setEngineKind(llvm::EngineKind::JIT).create();

    if(!engine){
        std::cerr << "Failed to create the Execution Engine: " << errStr << "\n";
        return;
    }
    
    if (auto* llvmPrint = modulePtr->getFunction("print")) {
        engine->addGlobalMapping(llvmPrint, (void*)&nova_print);
    } else {
        std::cerr << "Warning: 'print' not declared in module, announce()/print() calls will crash\n";
    }
    if (auto* fn = modulePtr->getFunction("nova_init_window")) {
        engine->addGlobalMapping(fn, (void*)&nova_init_window);
    }
    if (auto* fn = modulePtr->getFunction("nova_draw_rect")) {
        engine->addGlobalMapping(fn, (void*)&nova_draw_rect);
    }
    if (auto* fn = modulePtr->getFunction("nova_draw_rect_color")) {
        engine->addGlobalMapping(fn, (void*)&nova_draw_rect_color);
    }
    if (auto* fn = modulePtr->getFunction("nova_draw_text")) {
        engine->addGlobalMapping(fn, (void*)&nova_draw_text);
    }
    if (auto* fn = modulePtr->getFunction("nova_draw_number")) {
        engine->addGlobalMapping(fn, (void*)&nova_draw_number);
    }
    if (auto* fn = modulePtr->getFunction("nova_clear_screen")) {
        engine->addGlobalMapping(fn, (void*)&nova_clear_screen);
    }
    if (auto* fn = modulePtr->getFunction("nova_render_frame")) {
        engine->addGlobalMapping(fn, (void*)&nova_render_frame);
    }
    if (auto* fn = modulePtr->getFunction("nova_set_fps")) {
        engine->addGlobalMapping(fn, (void*)&nova_set_fps);
    }
    if (auto* fn = modulePtr->getFunction("nova_is_key_down")) {
        engine->addGlobalMapping(fn, (void*)&nova_is_key_down);
    }
    if (auto* fn = modulePtr->getFunction("nova_random")) {
        engine->addGlobalMapping(fn, (void*)&nova_random);
    }
    if (auto* fn = modulePtr->getFunction("nova_close_window")) {
        engine->addGlobalMapping(fn, (void*)&nova_close_window);
    }
    if (auto* fn = modulePtr->getFunction("nova_bounds_error")) {
        engine->addGlobalMapping(fn, (void*)&nova_bounds_error);
    }
    if (auto* fn = modulePtr->getFunction("nova_print_string")) {
        engine->addGlobalMapping(fn, (void*)&nova_print_string);
    }
    if (auto* fn = modulePtr->getFunction("nova_print_array")) {
        engine->addGlobalMapping(fn, (void*)&nova_print_array);
    }
    if (auto* fn = modulePtr->getFunction("nova_string_concat")) {
        engine->addGlobalMapping(fn, (void*)&nova_string_concat);
    }
    if (auto* fn = modulePtr->getFunction("nova_string_equals")) {
        engine->addGlobalMapping(fn, (void*)&nova_string_equals);
    }
    
    engine->finalizeObject();
    uint64_t mainAddress = engine->getFunctionAddress("nova_main");
    if(mainAddress == 0){
        std::cerr << "Couldn't find the compiled main function address. \n";
        return;
    }
    double (*nativeMain)() = (double (*)())mainAddress;
    nativeMain();
}
// --- Statement / Expression dispatch ---
// Node type is resolved at compile-time via the Visitor Pattern: accept()
// calls straight into the matching visitXxx override below — no RTTI, no
// dynamic_cast chain to walk at runtime.
void CodeGen::generateStatement(Stmt* stmt) {
    if (stmt) stmt->accept(*this);
}

llvm::Value* CodeGen::generateExpression(Expression* expr) {
    if (!expr) return nullptr;
    return expr->accept(*this);
}

// --- Statement Visitors ---
void CodeGen::visitLetStmt(LetStmt* letStmt) {
    llvm::Value* initValue = nullptr;
    if (letStmt->initializer) {
        initValue = generateExpression(letStmt->initializer.get());
    }

    // Every expression in this language produces either a double or (new)
    // a heap pointer — array literals and array_new() are the only sources
    // of the latter (see visitArrayLiteralExpr / the array_new builtin in
    // visitCallExpr). Size the slot to match whichever it is, so the load
    // in visitVariableExpr below can ask the slot what type it holds
    // instead of us having to track that separately.
    llvm::Type* slotType = (initValue && initValue->getType()->isPointerTy())
        ? builder->getPtrTy()
        : llvm::Type::getDoubleTy(*context);

    llvm::Value* slot = nullptr;

    if (env->isGlobalScope()) {
        // A top-level `loot` has to be reachable from every skill, not
        // just main() — a stack alloca here would only be valid IR inside
        // whichever function happens to be generating code right now
        // (main()), since SSA values are scoped to their defining
        // function. A GlobalVariable is the one kind of storage that's
        // legitimately usable from any function in the module, which is
        // exactly what's needed once skills read/write globals directly
        // (see check_self_collision()/shift_tail() etc. in snake.nv).
        llvm::Constant* zeroInit = slotType->isPointerTy()
            ? static_cast<llvm::Constant*>(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(slotType)))
            : static_cast<llvm::Constant*>(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
        llvm::GlobalVariable* gvar = new llvm::GlobalVariable(
            *module, slotType, /*isConstant=*/false,
            llvm::GlobalValue::InternalLinkage, zeroInit, letStmt->name
        );
        if (initValue) {
            builder->CreateStore(initValue, gvar);
        }
        slot = gvar;
    } else {
        llvm::AllocaInst* alloca = builder->CreateAlloca(slotType, nullptr, letStmt->name);
        if (initValue) {
            builder->CreateStore(initValue, alloca);
        }
        slot = alloca;
    }

    // Save the address in our Environment so we can find 'x' later
    env->define(letStmt->name, slot);
}

void CodeGen::visitExpressionStmt(ExpressionStmt* exprStmt) {
    generateExpression(exprStmt->expr.get());
}

void CodeGen::visitBlockStmt(BlockStmt* blockStmt) {
    // Give the block its own scope, chained to whatever scope was active
    // when we entered it. Without this, a `loot` declared inside a `when`
    // or `grind` body defines straight into the enclosing function (or
    // global) scope: two branches — or two loop iterations — declaring
    // the same name alias each other instead of shadowing, and the name
    // stays live (and keeps resolving) after the block ends. Mirrors the
    // save/restore pattern already used around function bodies in
    // visitFunctionStmt.
    std::shared_ptr<Environment> savedEnv = env;
    env = std::make_shared<Environment>(savedEnv);

    for (const auto& s : blockStmt->statements) {
        generateStatement(s.get());
        // `break`, `continue`, and `reward` all terminate the current
        // basic block (via CreateBr/CreateRet). LLVM requires exactly one
        // terminator per block, and anything textually after one of these
        // is unreachable anyway, so stop emitting here instead of trying
        // to append more instructions after a terminator.
        if (builder->GetInsertBlock()->getTerminator()) break;
    }

    env = savedEnv;
}

void CodeGen::visitIfStmt(IfStmt* ifStmt) {
    // 1. Evaluate the condition (returns 1.0 for true, 0.0 for false)
    llvm::Value* condVal = generateExpression(ifStmt->condition.get());
    if (!condVal) {
        std::cerr << "Error: 'when' condition failed to compile\n";
        return;
    }

    // Convert our double back to an LLVM boolean (i1) for the CPU branch
    llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    llvm::Value* condBool = builder->CreateFCmpONE(condVal, zero, "ifcond");

    // 2. Create the memory blocks for our branches
    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*context, "then", theFunction);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "merge");

    // 3. Tell the JIT to jump to 'then' if true, or 'else' (or merge) if false
    builder->CreateCondBr(condBool, thenBB, ifStmt->elseBranch ? elseBB : mergeBB);

    // --- GENERATE THEN BLOCK ---
    builder->SetInsertPoint(thenBB);
    generateStatement(ifStmt->thenBranch.get());
    if (!builder->GetInsertBlock()->getTerminator()) builder->CreateBr(mergeBB);

    // --- GENERATE ELSE BLOCK ---
    if (ifStmt->elseBranch) {
        theFunction->insert(theFunction->end(), elseBB);
        builder->SetInsertPoint(elseBB);
        generateStatement(ifStmt->elseBranch.get());
        if (!builder->GetInsertBlock()->getTerminator()) builder->CreateBr(mergeBB);
    }

    // --- RESUME MAIN SCRIPT ---
    theFunction->insert(theFunction->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
}

// --- LOOP (GRIND) ---
void CodeGen::visitWhileStmt(WhileStmt* whileStmt) {
    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();

    // Create the blocks for the loop cycle
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*context, "loopcond", theFunction);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*context, "loopbody");
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*context, "afterloop");

    // Jump from the current block into the condition block
    builder->CreateBr(condBB);
    builder->SetInsertPoint(condBB);

    // Evaluate condition
    llvm::Value* condVal = generateExpression(whileStmt->condition.get());
    if (!condVal) {
        std::cerr << "Error: 'grind' condition failed to compile\n";
        return;
    }
    llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    llvm::Value* condBool = builder->CreateFCmpONE(condVal, zero, "whilecond");
    builder->CreateCondBr(condBool, bodyBB, afterBB);

    // Generate the body, then jump back to the condition
    theFunction->insert(theFunction->end(), bodyBB);
    builder->SetInsertPoint(bodyBB);

    // 'break' jumps straight to afterBB (loop exit); 'continue' jumps back
    // to condBB (re-check the condition, matching normal C-style continue
    // semantics for a `while`). Pushed/popped as a stack so a `break`
    // inside a nested `grind` resolves to its own innermost loop, not an
    // outer one.
    breakTargets.push_back(afterBB);
    continueTargets.push_back(condBB);
    generateStatement(whileStmt->body.get());
    breakTargets.pop_back();
    continueTargets.pop_back();

    // The body may already end in a terminator if every path through it
    // hit break/continue/reward — don't add a second branch on top of it.
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(condBB);
    }

    // Resume script after the loop
    theFunction->insert(theFunction->end(), afterBB);
    builder->SetInsertPoint(afterBB);
}

// --- FUNCTION (SKILL) ---
void CodeGen::visitFunctionStmt(FunctionStmt* funcStmt) {
    // 1. Define the function signature (all doubles)
    std::vector<llvm::Type*> paramTypes(funcStmt->params.size(), llvm::Type::getDoubleTy(*context));
    llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getDoubleTy(*context), paramTypes, false);
    llvm::Function* func = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, funcStmt->name, module.get());

    // 2. Save where we were currently generating code (usually 'main')
    llvm::BasicBlock* oldInsertPoint = builder->GetInsertBlock();

    // 3. Create the new function block
    llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(*context, "entry", func);
    builder->SetInsertPoint(entryBB);

    // 4. Give the function body its own scope, chained to the enclosing
    // (global) one via Environment::enclosing. Without this, params and
    // any `loot` declared inside the body write straight into the shared
    // global env and are never removed — so a param named the same as a
    // global permanently clobbers it, and worse, later code anywhere in
    // the program can resolve that name to an alloca that lives in this
    // function's (already-returned-from) stack frame: invalid IR, not
    // just a logic bug.
    std::shared_ptr<Environment> savedEnv = env;
    env = std::make_shared<Environment>(savedEnv);

    // 5. Allocate memory for parameters and bind them to the new scope
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        arg.setName(funcStmt->params[idx]);
        llvm::AllocaInst* alloca = builder->CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, arg.getName());
        builder->CreateStore(&arg, alloca);
        env->define(funcStmt->params[idx], alloca); // This makes the params usable inside the function!
        idx++;
    }

    // 6. Generate the body — Environment::get() falls back to `enclosing`,
    // so the body can still read globals; it just can't clobber them.
    for (const auto& s : funcStmt->body) {
        generateStatement(s.get());
        // Same reasoning as visitBlockStmt: a `reward` (or, transitively,
        // a `break`/`continue` the function body shouldn't even reach
        // un-nested, but guard anyway) already terminated this block —
        // stop before appending anything after it.
        if (builder->GetInsertBlock()->getTerminator()) break;
    }

    // 6b. LLVM requires every basic block to end in a terminator
    // (ret/br/etc). A `skill` whose body falls off the end without an
    // explicit `reward` on every path — e.g. a function used purely for
    // side effects, like shift_tail() in snake.nv — would otherwise leave
    // this function's last block without one, which is invalid IR. Add an
    // implicit `reward 0` only if the current block doesn't already have
    // a terminator (an explicit `reward` earlier in this block already
    // added one via CreateRet, so this is a no-op in that case).
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateRet(llvm::ConstantFP::get(*context, llvm::APFloat(0.0)));
    }

    // 7. Leave the function's scope and return to compiling the caller
    env = savedEnv;
    if (oldInsertPoint) {
        builder->SetInsertPoint(oldInsertPoint);
    }
}

// --- RETURN (REWARD) ---
void CodeGen::visitReturnStmt(ReturnStmt* retStmt) {
    llvm::Value* retVal = nullptr;
    if (retStmt->value) {
        retVal = generateExpression(retStmt->value.get());
    } else {
        retVal = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    }
    // Emit the physical CPU return instruction
    builder->CreateRet(retVal);
}

// --- LOOP CONTROL (BREAK / CONTINUE) ---
// Both just branch to whatever the innermost active `grind` pushed onto
// the matching target stack in visitWhileStmt. An empty stack means this
// statement isn't lexically inside a loop at all — reported and otherwise
// ignored (no branch emitted), matching this file's existing style of
// printing to cerr and continuing rather than throwing (see
// Environment::get()'s "Undefined variable" case).
void CodeGen::visitBreakStmt(BreakStmt* stmt) {
    if (breakTargets.empty()) {
        std::cerr << "Error at " << stmt->keyword.line << ":" << stmt->keyword.column
                   << ": 'break' used outside of a loop\n";
        return;
    }
    builder->CreateBr(breakTargets.back());
}

void CodeGen::visitContinueStmt(ContinueStmt* stmt) {
    if (continueTargets.empty()) {
        std::cerr << "Error at " << stmt->keyword.line << ":" << stmt->keyword.column
                   << ": 'continue' used outside of a loop\n";
        return;
    }
    builder->CreateBr(continueTargets.back());
}

// --- Expression Visitors ---
llvm::Value* CodeGen::visitLiteralExpr(LiteralExpression* literal) {
    if (literal->value.type == TokenType::String) {
        // Strings share the array [tag][length][elements...] heap layout
        // (see the comment above nova_print_string near the top of this
        // file): tag=NOVA_TAG_STRING, length=character count, and each
        // element slot holds one character's code point as a double — a
        // string is, physically, just an array of char codes. Every byte
        // is known at compile time here, so each slot gets a direct store
        // rather than a runtime loop (contrast visitArrayLiteralExpr,
        // whose elements are arbitrary expressions that must be evaluated).
        const std::string& text = literal->value.value;
        uint64_t n = text.size();
        llvm::Value* byteSize = llvm::ConstantInt::get(sizeTy(), (n + NOVA_HEADER_SLOTS) * NOVA_ELEM_SIZE);

        llvm::FunctionCallee mallocFn = getMallocFn(module.get(), *builder, sizeTy());
        llvm::Value* strPtr = builder->CreateCall(mallocFn, {byteSize}, "str_ptr");

        builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(NOVA_TAG_STRING)), strPtr);
        llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), strPtr, builder->getInt64(1), "str_len_slot_ptr");
        builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat((double)n)), lenSlotPtr);

        for (uint64_t i = 0; i < n; ++i) {
            llvm::Value* charPtr = builder->CreateGEP(
                llvm::Type::getDoubleTy(*context), strPtr, builder->getInt64(i + NOVA_HEADER_SLOTS), "char_ptr"
            );
            builder->CreateStore(
                llvm::ConstantFP::get(*context, llvm::APFloat((double)(unsigned char)text[i])), charPtr
            );
        }
        return strPtr;
    }
    double val = std::stod(literal->value.value);
    return llvm::ConstantFP::get(*context, llvm::APFloat(val));
}

llvm::Value* CodeGen::visitBinaryExpr(BinaryExpression* binary) {
    llvm::Value* L = generateExpression(binary->left.get());
    llvm::Value* R = generateExpression(binary->right.get());

    if (!L || !R) return nullptr;

    bool bothPointers = L->getType()->isPointerTy() && R->getType()->isPointerTy();
    bool eitherPointer = L->getType()->isPointerTy() || R->getType()->isPointerTy();

    // Strings (and only strings — see the caveat below) support '+' as
    // concatenation and '=='/'!=' as content equality. Every other
    // operator, and every case where only one side is a pointer, falls
    // through to the "unsupported between these operand types" error
    // below rather than reaching the numeric switch, where e.g. CreateFSub
    // on a pointer-typed operand would be invalid IR (LLVM instructions
    // are strictly typed; there's no implicit pointer-to-double coercion).
    if (binary->op.type == TokenType::Plus && bothPointers) {
        // Doesn't itself verify both operands are tagged NOVA_TAG_STRING
        // rather than NOVA_TAG_ARRAY before calling nova_string_concat —
        // concatenating two arrays this way would reinterpret their
        // tag/length header as string data. Flagged as a follow-up
        // (mirroring nova_string_equals, which does check), not done here.
        return builder->CreateCall(getStringConcatFn(module.get(), *builder), {L, R}, "concat_tmp");
    }
    if ((binary->op.type == TokenType::equalequal || binary->op.type == TokenType::bangequal) && bothPointers) {
        llvm::Value* eq = builder->CreateCall(getStringEqualsFn(module.get(), *builder), {L, R}, "str_eq_tmp");
        if (binary->op.type == TokenType::bangequal) {
            llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
            llvm::Value* isZero = builder->CreateFCmpOEQ(eq, zero, "str_neq_bool");
            return builder->CreateUIToFP(isZero, llvm::Type::getDoubleTy(*context), "str_neq");
        }
        return eq;
    }
    if (eitherPointer) {
        std::cerr << "Error: operator '" << binary->op.value << "' isn't supported between these operand types\n";
        return nullptr;
    }

    switch (binary->op.type) {
        case TokenType::Plus:
            return builder->CreateFAdd(L, R, "addtmp");
        case TokenType::Minus:
            return builder->CreateFSub(L, R, "subtmp");
        case TokenType::Star:
            return builder->CreateFMul(L, R, "multmp");
        case TokenType::Slash:
            return builder->CreateFDiv(L, R, "divtmp");
        case TokenType::percent:
            // frem gives C/C++ fmod() semantics (result takes the sign of
            // the dividend), same convention as % in most languages Nova's
            // syntax otherwise resembles.
            return builder->CreateFRem(L, R, "modtmp");
        case TokenType::less:
            L = builder->CreateFCmpOLT(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::great:
            L = builder->CreateFCmpOGT(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::lessequal:
            L = builder->CreateFCmpOLE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::greatequal:
            L = builder->CreateFCmpOGE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::equalequal:
            L = builder->CreateFCmpOEQ(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::bangequal:
            L = builder->CreateFCmpONE(L, R, "cmptmp");
            return builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*context), "booltmp");
        case TokenType::Floor: {
            // Integer division: truncate toward zero after the FP divide,
            // matching what // reads as ("floor" in name only — this is
            // the common truncating-divide behavior, not true floor()
            // for negative operands; flag this if that distinction matters
            // to you later).
            llvm::Value* divRes = builder->CreateFDiv(L, R, "divtmp");
            llvm::Value* asInt = builder->CreateFPToSI(divRes, builder->getInt64Ty(), "floor_i64");
            return builder->CreateSIToFP(asInt, llvm::Type::getDoubleTy(*context), "floortmp");
        }
        default:
            std::cerr << "Invalid or unsupported binary operator\n";
            return nullptr;
    }
}

// NOTE: unary operators had no branch in the old dynamic_cast chain (an
// expression like `-x` or `!x` silently fell through to "Unknown expression
// type"). The Visitor interface requires every node type to be handled, so
// this fills that gap with minimal support for negation and logical-not —
// worth a look to confirm it matches the language's intended semantics.
llvm::Value* CodeGen::visitUnaryExpr(UnaryExpression* unary) {
    llvm::Value* operand = generateExpression(unary->right.get());
    if (!operand) return nullptr;

    if (unary->op.value == "-") {
        return builder->CreateFNeg(operand, "negtmp");
    }
    if (unary->op.value == "!") {
        llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
        llvm::Value* isZero = builder->CreateFCmpOEQ(operand, zero, "nottmp");
        return builder->CreateUIToFP(isZero, llvm::Type::getDoubleTy(*context), "boolnot");
    }

    std::cerr << "Invalid or unsupported unary operator\n";
    return nullptr;
}

llvm::Value* CodeGen::visitGroupingExpr(GroupingExpression* grouping) {
    // Parentheses don't change the value, only parse precedence.
    return generateExpression(grouping->inner.get());
}

llvm::Value* CodeGen::visitVariableExpr(VariableExpression* varNode) {
    llvm::Value* slot = env->get(varNode->name);
    if (!slot) return nullptr;
    llvm::Type* slotType = getSlotType(slot);
    if (!slotType) {
        std::cerr << "Internal error: unrecognized storage kind for '" << varNode->name << "'\n";
        return nullptr;
    }
    // Ask the slot what it actually holds (double or an array pointer)
    // rather than hardcoding double, since letStmt now allocas/globals
    // either kind.
    return builder->CreateLoad(slotType, slot, varNode->name + "_val");
}

llvm::Value* CodeGen::visitAssignmentExpr(AssignmentExpression* assign) {
    llvm::Value* val = generateExpression(assign->value.get());
    if (!val) return nullptr;

    llvm::Value* slot = env->get(assign->name);
    if (!slot) {
        std::cerr << "Undefined variable for equip: " << assign->name << "\n";
        return nullptr;
    }

    builder->CreateStore(val, slot);
    return val;
}

// Real short-circuit evaluation, mirroring visitIfStmt's branch structure.
// This matters more than it used to: with arrays in the language, a guard
// like `when (i < len and arr[i] > 0)` needs the right side to never even
// evaluate once the left side is false, or it reads out of bounds.
llvm::Value* CodeGen::visitLogicalExpr(LogicalExpression* logical) {
    bool isAnd = (logical->op.value == "&&" || logical->op.value == "and");
    bool isOr  = (logical->op.value == "||" || logical->op.value == "or");
    if (!isAnd && !isOr) {
        std::cerr << "Invalid or unsupported logical operator\n";
        return nullptr;
    }

    llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    llvm::Value* L = generateExpression(logical->left.get());
    if (!L) return nullptr;
    llvm::Value* Lbool = builder->CreateFCmpONE(L, zero, "lbool");

    // Slot to hold whichever operand's value actually ends up mattering —
    // same alloca-then-merge pattern used for returnAlloc, rather than a
    // PHI node, to match this file's existing style.
    llvm::AllocaInst* resultAlloca = builder->CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, "logical_tmp");

    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhsBB    = llvm::BasicBlock::Create(*context, isAnd ? "and_rhs" : "or_rhs");
    llvm::BasicBlock* skipBB   = llvm::BasicBlock::Create(*context, isAnd ? "and_skip" : "or_skip");
    llvm::BasicBlock* mergeBB  = llvm::BasicBlock::Create(*context, "logical_merge");

    // AND: right side only matters (and only runs) if the left was true.
    // OR:  right side only matters (and only runs) if the left was false.
    if (isAnd) {
        builder->CreateCondBr(Lbool, rhsBB, skipBB);
    } else {
        builder->CreateCondBr(Lbool, skipBB, rhsBB);
    }

    // --- skip: short-circuited, left side alone decides the result ---
    theFunction->insert(theFunction->end(), skipBB);
    builder->SetInsertPoint(skipBB);
    builder->CreateStore(L, resultAlloca);
    builder->CreateBr(mergeBB);

    // --- rhs: only reached, and only evaluated, when actually needed ---
    theFunction->insert(theFunction->end(), rhsBB);
    builder->SetInsertPoint(rhsBB);
    llvm::Value* R = generateExpression(logical->right.get());
    if (!R) return nullptr;
    builder->CreateStore(R, resultAlloca);
    builder->CreateBr(mergeBB);

    // --- merge: normalize to a canonical 0.0/1.0, same as every other
    // comparison/logical operator in this language (not a raw passthrough
    // of whichever operand's value was used) ---
    theFunction->insert(theFunction->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
    llvm::Value* merged = builder->CreateLoad(llvm::Type::getDoubleTy(*context), resultAlloca, "logical_result");
    llvm::Value* mergedBool = builder->CreateFCmpONE(merged, zero, "logical_bool");
    return builder->CreateUIToFP(mergedBool, llvm::Type::getDoubleTy(*context), "logical_final");
}

llvm::Value* CodeGen::visitCallExpr(CallExpression* call) {
    auto* varNode = dynamic_cast<VariableExpression*>(call->callee.get());
    if (!varNode) return nullptr;

    // --- STANDARD LIBRARY HIJACK ---
    if (varNode->name == "announce" && !call->args.empty()) {
        llvm::Value* argVal = generateExpression(call->args[0].get());
        if (!argVal) return nullptr;

        if (argVal->getType()->isPointerTy()) {
            // Arrays and strings are both heap pointers at the LLVM level
            // (see the layout comment above nova_print_string near the top
            // of this file) — which one this actually is can only be
            // determined at RUNTIME by reading the tag in slot 0, since
            // Nova has no static type system: the same call site could see
            // either kind depending on what ran earlier.
            llvm::Value* tagVal = builder->CreateLoad(llvm::Type::getDoubleTy(*context), argVal, "print_tag");
            llvm::Value* isString = builder->CreateFCmpOEQ(
                tagVal, llvm::ConstantFP::get(*context, llvm::APFloat(NOVA_TAG_STRING)), "is_string"
            );

            llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
            llvm::BasicBlock* strBB = llvm::BasicBlock::Create(*context, "print_str", theFunction);
            llvm::BasicBlock* arrBB = llvm::BasicBlock::Create(*context, "print_arr");
            llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "print_merge");
            builder->CreateCondBr(isString, strBB, arrBB);

            builder->SetInsertPoint(strBB);
            builder->CreateCall(getPrintStringFn(module.get(), *builder), {argVal});
            builder->CreateBr(mergeBB);

            theFunction->insert(theFunction->end(), arrBB);
            builder->SetInsertPoint(arrBB);
            builder->CreateCall(getPrintArrayFn(module.get(), *builder), {argVal});
            builder->CreateBr(mergeBB);

            theFunction->insert(theFunction->end(), mergeBB);
            builder->SetInsertPoint(mergeBB);
            return argVal;
        }

        llvm::Function* printFn = module->getFunction("print");
        if (!printFn) {
            std::cerr << "Internal error: 'print' function not declared in module\n";
            return nullptr;
        }
        builder->CreateCall(printFn, {argVal});
        return argVal;
    }
    if (varNode->name == "array_new" && call->args.size() == 1) {
        // Zero-initialized heap array of `size` doubles — the pattern for a
        // preallocated, growable buffer (e.g. a snake's tail): allocate a
        // generous capacity up front, track how many slots are in use with
        // an ordinary `loot len = 0` variable, and index/assign into it.
        // Physically allocates size+2 slots: slot 0 is the tag (see the
        // layout comment near nova_print_string above), slot 1 holds the
        // length (used for bounds checking — see visitIndexExpr/
        // visitIndexAssignExpr and the length() builtin), slots 2..size+1
        // hold the elements.
        llvm::Value* sizeVal = generateExpression(call->args[0].get());
        if (!sizeVal) return nullptr;
        llvm::Value* sizeInt = builder->CreateFPToUI(sizeVal, sizeTy(), "arr_len");
        llvm::Value* totalSlots = builder->CreateAdd(sizeInt, llvm::ConstantInt::get(sizeTy(), NOVA_HEADER_SLOTS), "arr_total_slots");

        llvm::FunctionCallee callocFn = getCallocFn(module.get(), *builder, sizeTy());
        llvm::Value* arrPtr = builder->CreateCall(callocFn, {totalSlots, llvm::ConstantInt::get(sizeTy(), NOVA_ELEM_SIZE)}, "arr_ptr");
        // calloc zeroes everything, which already gives slot 0 the
        // NOVA_TAG_ARRAY bit pattern (0.0 is all-zero bits under IEEE754)
        // and zeroes every element slot correctly either way — only the
        // length header at slot 1 needs an explicit store.
        llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(1), "len_slot_ptr");
        builder->CreateStore(sizeVal, lenSlotPtr);
        return arrPtr;
    }
    if (varNode->name == "length" && call->args.size() == 1) {
        // Slot 1 is the length header (slot 0 is the tag — see the layout
        // comment near nova_print_string above). Works on strings too,
        // since they share this layout.
        llvm::Value* arrPtr = generateExpression(call->args[0].get());
        if (!arrPtr) return nullptr;
        if (!arrPtr->getType()->isPointerTy()) {
            std::cerr << "Error: length() expects an array or string\n";
            return nullptr;
        }
        llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(1), "len_slot_ptr");
        return builder->CreateLoad(llvm::Type::getDoubleTy(*context), lenSlotPtr, "arr_len_val");
    }
    if ((varNode->name == "free_array" || varNode->name == "free_string") && call->args.size() == 1) {
        // Releases a heap array or string made by array_new()/an array
        // literal/a string literal/string concatenation. The language has
        // no GC or refcounting, so nothing does this automatically — call
        // it once you're done, especially for something allocated inside a
        // loop (e.g. per-frame in a `grind`, or a `+` concatenation
        // result), or the process leaks for as long as it runs. `free_array`
        // and `free_string` are the same operation under the hood (`free()`
        // doesn't care about the tag) — both names are provided since which
        // one reads naturally depends on what you're freeing. Using the
        // value (indexing, length(), printing, concatenating) after
        // freeing it is undefined behavior, same as in C — the language
        // doesn't track liveness to catch that for you.
        llvm::Value* arrPtr = generateExpression(call->args[0].get());
        if (!arrPtr) return nullptr;
        if (!arrPtr->getType()->isPointerTy()) {
            std::cerr << "Error: " << varNode->name << "() expects an array or string\n";
            return nullptr;
        }
        llvm::FunctionCallee freeFn = getFreeFn(module.get(), *builder);
        builder->CreateCall(freeFn, {arrPtr});
        return llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    }
    if (varNode->name == "init_window") {
        if (call->args.size() != 3) {
            std::cerr << "init_window expects 3 arguments: init_window(width, height, title)\n";
            return nullptr;
        }
        llvm::Value* w = generateExpression(call->args[0].get());
        llvm::Value* h = generateExpression(call->args[1].get());

        // The title must be a string literal today: the language has no
        // general string value type yet (LiteralExpression codegen only
        // ever produces doubles), so we pull the raw text straight off the
        // literal token here and bake it into an LLVM global string
        // constant, rather than routing it through generateExpression().
        auto* titleLit = dynamic_cast<LiteralExpression*>(call->args[2].get());
        if (!titleLit || titleLit->value.type != TokenType::String) {
            std::cerr << "init_window's third argument must be a string literal, e.g. init_window(800, 600, \"My Game\")\n";
            return nullptr;
        }
        llvm::Value* titlePtr = builder->CreateGlobalString(titleLit->value.value, "win_title");
        llvm::FunctionType* ft = llvm::FunctionType::get(
            builder->getDoubleTy(),
            {builder->getDoubleTy(), builder->getDoubleTy(), builder->getPtrTy()},
            false
        );
        llvm::FunctionCallee nativeInit = module->getOrInsertFunction("nova_init_window", ft);
        return builder->CreateCall(nativeInit, {w, h, titlePtr});
    }
    if (varNode->name == "set_fps" && call->args.size() == 1) {
        llvm::Value* fps = generateExpression(call->args[0].get());

        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), {builder->getDoubleTy()}, false);
        llvm::FunctionCallee nativeSetFps = module->getOrInsertFunction("nova_set_fps", ft);
        return builder->CreateCall(nativeSetFps, {fps});
    }
    // Was previously implemented natively (nova_is_key_down wraps raylib's
    // IsKeyDown) but had no hijack here, so it was unreachable from any
    // .nv script — no way to actually call it. Raylib key codes have to be
    // passed as raw numbers (e.g. 262 for KEY_RIGHT) since the language has
    // no named constants yet.
    if (varNode->name == "is_key_down" && call->args.size() == 1) {
        llvm::Value* keycode = generateExpression(call->args[0].get());
        if (!keycode) return nullptr;

        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), {builder->getDoubleTy()}, false);
        llvm::FunctionCallee nativeIsKeyDown = module->getOrInsertFunction("nova_is_key_down", ft);
        return builder->CreateCall(nativeIsKeyDown, {keycode});
    }
    if (varNode->name == "random" && call->args.size() == 2) {
        llvm::Value* minVal = generateExpression(call->args[0].get());
        llvm::Value* maxVal = generateExpression(call->args[1].get());
        if (!minVal || !maxVal) return nullptr;

        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), {builder->getDoubleTy(), builder->getDoubleTy()}, false);
        llvm::FunctionCallee nativeRandom = module->getOrInsertFunction("nova_random", ft);
        return builder->CreateCall(nativeRandom, {minVal, maxVal});
    }
    if (varNode->name == "clear_screen") {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), false);
        llvm::FunctionCallee nativeClear = module->getOrInsertFunction("nova_clear_screen", ft);
        return builder->CreateCall(nativeClear);
    }
    if (varNode->name == "render_frame") {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), false);
        llvm::FunctionCallee nativeRender = module->getOrInsertFunction("nova_render_frame", ft);
        return builder->CreateCall(nativeRender);
    }
    if (varNode->name == "close_window") {
        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), false);
        llvm::FunctionCallee nativeClose = module->getOrInsertFunction("nova_close_window", ft);
        return builder->CreateCall(nativeClose);
    }
    if (varNode->name == "draw_rect" && call->args.size() == 4) {
        llvm::Value* x = generateExpression(call->args[0].get());
        llvm::Value* y = generateExpression(call->args[1].get());
        llvm::Value* w = generateExpression(call->args[2].get());
        llvm::Value* h = generateExpression(call->args[3].get());

        llvm::FunctionType* ft = llvm::FunctionType::get(builder->getDoubleTy(), {builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy()}, false);
        llvm::FunctionCallee nativeDraw = module->getOrInsertFunction("nova_draw_rect", ft);
        return builder->CreateCall(nativeDraw, {x, y, w, h});
    }
    // draw_rect_color(x, y, w, h, r, g, b): same as draw_rect but with an
    // explicit 0-255 RGB color, so scripts can visually distinguish game
    // elements (snake body, food, walls, UI panels) instead of everything
    // being a plain white rectangle.
    if (varNode->name == "draw_rect_color" && call->args.size() == 7) {
        llvm::Value* x = generateExpression(call->args[0].get());
        llvm::Value* y = generateExpression(call->args[1].get());
        llvm::Value* w = generateExpression(call->args[2].get());
        llvm::Value* h = generateExpression(call->args[3].get());
        llvm::Value* r = generateExpression(call->args[4].get());
        llvm::Value* g = generateExpression(call->args[5].get());
        llvm::Value* b = generateExpression(call->args[6].get());

        llvm::FunctionType* ft = llvm::FunctionType::get(
            builder->getDoubleTy(),
            {builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy(),
             builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy()},
            false
        );
        llvm::FunctionCallee nativeDrawColor = module->getOrInsertFunction("nova_draw_rect_color", ft);
        return builder->CreateCall(nativeDrawColor, {x, y, w, h, r, g, b});
    }
    // draw_text(x, y, size, textExpr): textExpr must evaluate to a Nova
    // string (a string literal, a `loot` holding one, or a concatenation) —
    // same pointer-passing approach as announce()/free_string, just routed
    // to raylib's DrawText instead of stdout.
    if (varNode->name == "draw_text" && call->args.size() == 4) {
        llvm::Value* x = generateExpression(call->args[0].get());
        llvm::Value* y = generateExpression(call->args[1].get());
        llvm::Value* size = generateExpression(call->args[2].get());
        llvm::Value* strPtr = generateExpression(call->args[3].get());
        if (!x || !y || !size || !strPtr) return nullptr;
        if (!strPtr->getType()->isPointerTy()) {
            std::cerr << "Error: draw_text()'s 4th argument must be a string\n";
            return nullptr;
        }

        llvm::FunctionType* ft = llvm::FunctionType::get(
            builder->getDoubleTy(),
            {builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy(), builder->getPtrTy()},
            false
        );
        llvm::FunctionCallee nativeDrawText = module->getOrInsertFunction("nova_draw_text", ft);
        return builder->CreateCall(nativeDrawText, {x, y, size, strPtr});
    }
    // draw_number(x, y, size, value): like draw_text but for a raw number —
    // no Nova string involved at all, so there's no string-concatenation
    // ("+") needed just to put a score on screen.
    if (varNode->name == "draw_number" && call->args.size() == 4) {
        llvm::Value* x = generateExpression(call->args[0].get());
        llvm::Value* y = generateExpression(call->args[1].get());
        llvm::Value* size = generateExpression(call->args[2].get());
        llvm::Value* value = generateExpression(call->args[3].get());
        if (!x || !y || !size || !value) return nullptr;

        llvm::FunctionType* ft = llvm::FunctionType::get(
            builder->getDoubleTy(),
            {builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy(), builder->getDoubleTy()},
            false
        );
        llvm::FunctionCallee nativeDrawNumber = module->getOrInsertFunction("nova_draw_number", ft);
        return builder->CreateCall(nativeDrawNumber, {x, y, size, value});
    }

    // --- CUSTOM SKILL EXECUTION ---
    llvm::Function* calleeF = module->getFunction(varNode->name);
    if (!calleeF) {
        std::cerr << "Unknown skill: " << varNode->name << "\n";
        return nullptr;
    }

    // Evaluate all arguments
    std::vector<llvm::Value*> argsV;
    for (const auto& arg : call->args) {
        argsV.push_back(generateExpression(arg.get()));
    }

    // Emit the call instruction to jump to our custom skill
    return builder->CreateCall(calleeF, argsV, "calltmp");
}

// --- ARRAYS (heap-backed, with a tag+length header — see the layout
// comment above nova_print_string near the top of this file) ---
// [e1, e2, ...] : malloc (n+2)*8 bytes — slot 0 is the tag, slot 1 is the
// length header, slots 2..n+1 are the elements — evaluate + store each
// element in order, return the base pointer. Same physical layout
// array_new() produces above, so both paths feed the exact same indexing
// codegen below.
llvm::Value* CodeGen::visitArrayLiteralExpr(ArrayLiteralExpression* expr) {
    uint64_t n = expr->elements.size();
    llvm::Value* byteSize = llvm::ConstantInt::get(sizeTy(), (n + NOVA_HEADER_SLOTS) * NOVA_ELEM_SIZE);

    llvm::FunctionCallee mallocFn = getMallocFn(module.get(), *builder, sizeTy());
    llvm::Value* arrPtr = builder->CreateCall(mallocFn, {byteSize}, "arr_ptr");

    // Slot 0: tag (this is an array, not a string)
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat(NOVA_TAG_ARRAY)), arrPtr);
    // Slot 1: length header
    llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(1), "len_slot_ptr");
    builder->CreateStore(llvm::ConstantFP::get(*context, llvm::APFloat((double)n)), lenSlotPtr);

    for (uint64_t i = 0; i < n; ++i) {
        llvm::Value* elemVal = generateExpression(expr->elements[i].get());
        if (!elemVal) continue;
        llvm::Value* elemPtr = builder->CreateGEP(
            llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(i + NOVA_HEADER_SLOTS), "elem_ptr"
        );
        builder->CreateStore(elemVal, elemPtr);
    }
    return arrPtr;
}

// arr[index] — bounds-checked against the length stored in slot 1 (slot 0
// is the array/string tag — see the layout comment above nova_print_string).
// Out of bounds: reports via nova_bounds_error and yields 0.0 rather than
// reading unrelated heap memory. Works unmodified on Nova strings too:
// indexing a string returns the character code at that position, since
// strings physically ARE arrays of character codes.
llvm::Value* CodeGen::visitIndexExpr(IndexExpression* expr) {
    llvm::Value* arrPtr = generateExpression(expr->array.get());
    llvm::Value* idxVal = generateExpression(expr->index.get());
    if (!arrPtr || !idxVal) return nullptr;

    if (!arrPtr->getType()->isPointerTy()) {
        std::cerr << "Error at " << expr->bracket.line << ":" << expr->bracket.column
                   << ": can't index a non-array value\n";
        return nullptr;
    }

    llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(1), "len_slot_ptr");
    llvm::Value* lenVal = builder->CreateLoad(llvm::Type::getDoubleTy(*context), lenSlotPtr, "arr_len_val");
    llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    llvm::Value* geZero = builder->CreateFCmpOGE(idxVal, zero, "idx_ge0");
    llvm::Value* ltLen = builder->CreateFCmpOLT(idxVal, lenVal, "idx_lt_len");
    llvm::Value* inBounds = builder->CreateAnd(geZero, ltLen, "in_bounds");

    llvm::AllocaInst* resultAlloca = builder->CreateAlloca(llvm::Type::getDoubleTy(*context), nullptr, "idx_result_tmp");

    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*context, "idx_ok", theFunction);
    llvm::BasicBlock* errBB = llvm::BasicBlock::Create(*context, "idx_err");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "idx_merge");
    builder->CreateCondBr(inBounds, okBB, errBB);

    builder->SetInsertPoint(okBB);
    llvm::Value* physIdx = builder->CreateFAdd(idxVal, llvm::ConstantFP::get(*context, llvm::APFloat((double)NOVA_HEADER_SLOTS)), "phys_idx");
    llvm::Value* physIdxInt = builder->CreateFPToSI(physIdx, builder->getInt64Ty(), "phys_idx_i64");
    llvm::Value* elemPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, physIdxInt, "elem_ptr");
    llvm::Value* elemVal = builder->CreateLoad(llvm::Type::getDoubleTy(*context), elemPtr, "elem_val");
    builder->CreateStore(elemVal, resultAlloca);
    builder->CreateBr(mergeBB);

    theFunction->insert(theFunction->end(), errBB);
    builder->SetInsertPoint(errBB);
    llvm::FunctionCallee boundsErrFn = getBoundsErrorFn(module.get(), *builder);
    builder->CreateCall(boundsErrFn, {idxVal, lenVal});
    builder->CreateStore(zero, resultAlloca);
    builder->CreateBr(mergeBB);

    theFunction->insert(theFunction->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
    return builder->CreateLoad(llvm::Type::getDoubleTy(*context), resultAlloca, "idx_result");
}

// arr[index] = value — same bounds check as above; out of bounds reports
// and skips the store. Like AssignmentExpression, still evaluates to the
// (attempted) value so `x = arr[i] = 5` style chaining keeps working.
llvm::Value* CodeGen::visitIndexAssignExpr(IndexAssignmentExpression* expr) {
    llvm::Value* arrPtr = generateExpression(expr->array.get());
    llvm::Value* idxVal = generateExpression(expr->index.get());
    llvm::Value* newVal = generateExpression(expr->value.get());
    if (!arrPtr || !idxVal || !newVal) return nullptr;

    if (!arrPtr->getType()->isPointerTy()) {
        std::cerr << "Error at " << expr->bracket.line << ":" << expr->bracket.column
                   << ": can't index a non-array value\n";
        return nullptr;
    }

    llvm::Value* lenSlotPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, builder->getInt64(1), "len_slot_ptr");
    llvm::Value* lenVal = builder->CreateLoad(llvm::Type::getDoubleTy(*context), lenSlotPtr, "arr_len_val");
    llvm::Value* zero = llvm::ConstantFP::get(*context, llvm::APFloat(0.0));
    llvm::Value* geZero = builder->CreateFCmpOGE(idxVal, zero, "idx_ge0");
    llvm::Value* ltLen = builder->CreateFCmpOLT(idxVal, lenVal, "idx_lt_len");
    llvm::Value* inBounds = builder->CreateAnd(geZero, ltLen, "in_bounds");

    llvm::Function* theFunction = builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* okBB = llvm::BasicBlock::Create(*context, "idx_store_ok", theFunction);
    llvm::BasicBlock* errBB = llvm::BasicBlock::Create(*context, "idx_store_err");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context, "idx_store_merge");
    builder->CreateCondBr(inBounds, okBB, errBB);

    builder->SetInsertPoint(okBB);
    llvm::Value* physIdx = builder->CreateFAdd(idxVal, llvm::ConstantFP::get(*context, llvm::APFloat((double)NOVA_HEADER_SLOTS)), "phys_idx");
    llvm::Value* physIdxInt = builder->CreateFPToSI(physIdx, builder->getInt64Ty(), "phys_idx_i64");
    llvm::Value* elemPtr = builder->CreateGEP(llvm::Type::getDoubleTy(*context), arrPtr, physIdxInt, "elem_ptr");
    builder->CreateStore(newVal, elemPtr);
    builder->CreateBr(mergeBB);

    theFunction->insert(theFunction->end(), errBB);
    builder->SetInsertPoint(errBB);
    llvm::FunctionCallee boundsErrFn = getBoundsErrorFn(module.get(), *builder);
    builder->CreateCall(boundsErrFn, {idxVal, lenVal});
    builder->CreateBr(mergeBB);

    theFunction->insert(theFunction->end(), mergeBB);
    builder->SetInsertPoint(mergeBB);
    return newVal;
}