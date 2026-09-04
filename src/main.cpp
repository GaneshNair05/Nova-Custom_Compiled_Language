#include <fstream> 
#include <iostream>
#include <string>
#include <vector> 
#include <sstream> 
#include "codegen/codegen.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include <llvm/IR/Module.h>

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
int main(int argc, char* argv[]){
    if (argc < 2){
        std::cerr << "Usage: mycompiler <source-file> \n";
        return 1; 
    }
    std::string filename = argv[1];
    std::ifstream file(filename);

    if(!file){
        std::cerr << "Error couldn't open file " << filename << "\n";
        return 1;
    };
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

    // Run the pipeline exactly ONCE
    Parser parser(tokens);
    std::vector<StmtPtr> statements = parser.parse();
    
    std::cout <<"\n--- Generating LLVM IR ---\n";
    CodeGen generator("NovaModule");
    generator.generate(statements);

    std::cout <<"\n--- Executing Program ---\n";
    generator.execute();

    return 0;
}