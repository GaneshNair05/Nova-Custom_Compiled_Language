cmake_minimum_required(VERSION 3.10)
project(MyCompiler)

set(CMAKE_CXX_STANDARD 17)

# Find LLVM package
find_package(ZLIB REQUIRED)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})


llvm_map_components_to_libnames(llvm_libs Core ExecutionEngine MCJIT OrcJIT Support native)

# Add your source files (update these paths based on your folder structure)
add_executable(mycompiler
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/codegen/codegen.cpp
)
target_link_libraries(mycompiler PRIVATE ${llvm_libs} raylib)
