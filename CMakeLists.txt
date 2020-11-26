cmake_minimum_required(VERSION 3.4.3)

project(OrbCompiler)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

set(HEADER_FILES
    "src/ClangAdapter.h"
    "src/CodeLoc.h"
    "src/CompilationOrchestrator.h"
    "src/CompilationMessages.h"
    "src/Compiler.h"
    "src/Evaluator.h"
    "src/EvalVal.h"
    "src/Lexer.h"
    "src/LiteralVal.h"
    "src/LlvmVal.h"
    "src/NamePool.h"
    "src/NodeVal.h"
    "src/Parser.h"
    "src/Processor.h"
    "src/Reserved.h"
    "src/StringPool.h"
    "src/SymbolTable.h"
    "src/Token.h"
    "src/TypeTable.h"
    "src/utils.h"
    )

set(SOURCE_FILES
    "src/ClangAdapter.cpp"
    "src/CompilationOrchestrator.cpp"
    "src/CompilationMessages.cpp"
    "src/Compiler.cpp"
    "src/Evaluator.cpp"
    "src/EvalVal.cpp"
    "src/Lexer.cpp"
    "src/main.cpp"
    "src/LiteralVal.cpp"
    "src/LlvmVal.cpp"
    "src/NamePool.cpp"
    "src/NodeVal.cpp"
    "src/Parser.cpp"
    "src/Processor.cpp"
    "src/Reserved.cpp"
    "src/StringPool.cpp"
    "src/SymbolTable.cpp"
    "src/Token.cpp"
    "src/TypeTable.cpp"
    "src/utils.cpp"
    )

add_executable(orbc "${HEADER_FILES}" "${SOURCE_FILES}")

llvm_map_components_to_libnames(LLVM_LIBS
    support
    core
    irreader
    aarch64asmparser
    aarch64codegen
    amdgpuasmparser
    amdgpucodegen
    armasmparser
    armcodegen
    avrasmparser
    avrcodegen
    bpfasmparser
    bpfcodegen
    hexagonasmparser
    hexagoncodegen
    lanaiasmparser
    lanaicodegen
    mipsasmparser
    mipscodegen
    msp430asmparser
    msp430codegen
    nvptxcodegen
    powerpcasmparser
    powerpccodegen
    riscvasmparser
    riscvcodegen
    sparcasmparser
    sparccodegen
    systemzasmparser
    systemzcodegen
    webassemblyasmparser
    webassemblycodegen
    x86asmparser
    x86codegen
    xcorecodegen
    )

target_link_libraries(orbc stdc++fs)

# Link against LLVM libraries
target_link_libraries(orbc LLVM)

target_link_libraries(orbc clangTooling)
