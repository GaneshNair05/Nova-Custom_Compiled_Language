#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>


namespace llvm { class Value; }

// Stores either an AllocaInst* (a function-local stack slot: skill params,
// skill-local `loot`s) or a GlobalVariable* (a top-level `loot`, valid from
// any function in the module) — both are pointer-typed llvm::Value, and
// CreateLoad/CreateStore don't care which one they're pointing at. The
// distinction only matters at the two places that create/read the slot's
// *type* (CodeGen::visitLetStmt and the getSlotType() helper in codegen.cpp).
class Environment {
public:
    Environment(std::shared_ptr<Environment> enclosing = nullptr)
        : enclosing(enclosing) {}

    void define(const std::string& name, llvm::Value* value) {
        values[name] = value;
    }

    llvm::Value* get(const std::string& name) {
        if (values.find(name) != values.end()) {
            return values[name];
        }
        if (enclosing != nullptr) {
            return enclosing->get(name);
        }
        std::cerr << "Undefined variable '" << name << "'\n";
        return nullptr;
    }

    // True only for the single root Environment created once in CodeGen's
    // constructor. Used by visitLetStmt to decide whether a `loot` needs
    // to become a real LLVM GlobalVariable (reachable from every skill)
    // rather than a stack alloca local to whichever function is currently
    // being generated.
    bool isGlobalScope() const { return enclosing == nullptr; }

private:
    std::unordered_map<std::string, llvm::Value*> values;
    std::shared_ptr<Environment> enclosing; // Points to the parent scope
};