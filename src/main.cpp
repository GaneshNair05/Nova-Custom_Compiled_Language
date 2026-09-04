#include <fstream> 
#include <iostream>
#include <string>
#include <vector> 
#include <sstream> 
#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

// LLVM Bitcode & FileSystem headers
#include <llvm/IR/Module.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

// Assuming 'TheModule' is your std::unique_ptr<llvm::Module>
void printLLVMIRAndExecute(llvm::Module* module) {
    // 1. Output the exact IR delimiter required by runNova.js
   std::cout << "\n--- Generating LLVM IR ---\n" << std::flush;

// Print module to LLVM's raw stdout stream
    module->print(llvm::outs(), nullptr);

// Force LLVM to flush its buffer to the OS stream immediately:
    llvm::outs().flush();

    std::cout << "\n--- Executing Program ---\n" << std::flush;

    // 4. Now invoke your execution engine / JIT to run the code
    // runJIT();
}

void emitWasmBitcode(llvm::Module* module, const std::string& outputPath) {
    // 1. Configure target triple & data layout for Emscripten 32-bit WebAssembly
    module->setTargetTriple("wasm32-unknown-emscripten");
    module->setDataLayout("e-m:e-p:32:32-p10:32:32-p20:32:32-i64:64-i128:128-f64:64-f80:128-n8:16:32:64-S128-ni:1:10:20");

    std::error_code ec;
    llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Error opening bitcode output file " << outputPath << ": " << ec.message() << "\n";
        return;
    }

    // 2. Write bitcode to disk
    llvm::WriteBitcodeToFile(*module, dest);
    dest.flush();
    std::cout << "\nWasm bitcode emitted successfully to: " << outputPath << "\n";
}

int main(int argc, char* argv[]){
    if (argc < 2){
        std::cerr << "Usage: mycompiler <source-file> [--emit-bc <output.bc>]\n";
        return 1; 
    }

    std::string filename = argv[1];
    bool emitBc = false;
    std::string bcOutputFile = "snake.bc";

    // Parse flags
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--emit-bc" && i + 1 < argc) {
            emitBc = true;
            bcOutputFile = argv[++i];
        }
    }

    std::ifstream file(filename);
    if(!file){
        std::cerr << "Error couldn't open file " << filename << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    
    for (const Token& token : tokens) {
        std::cout << tokenTypeToString(token.type)
                  << " : "
                  << token.value
                  << '\n';
    }

    if (lexer.hadError()) {
        std::cerr << "\nLexing failed; not proceeding to parse/codegen.\n";
        return 1;
    }

    // Run the pipeline
    Parser parser(tokens);
    std::vector<StmtPtr> statements = parser.parse();
    
    std::cout << "\n--- Generating LLVM IR ---\n";
    // wasmTarget = emitBc: when we're about to write out a .bc file for the
    // browser build, malloc/calloc need to be declared with wasm32's 32-bit
    // size_t, not the 64-bit one the native JIT path (execute(), below)
    // actually links against. See CodeGen::sizeTy() in codegen.cpp.
    CodeGen generator("NovaModule", emitBc);
    generator.generate(statements);

    // If --emit-bc was passed, write out the .bc file and exit
    if (emitBc) {
        emitWasmBitcode(generator.getModule(), bcOutputFile);
        return 0; 
    }

    // Default: run with local JIT execution engine
    std::cout << "\n--- Executing Program ---\n";
    generator.execute();

    return 0;
}