cmake_minimum_required(VERSION 3.4.3)

project(OrbCompiler)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

set(HEADER_FILES
    "include/ClangAdapter.h"
    "include/Codegen.h"
    "include/CodeLoc.h"
    "include/CompileMessages.h"
    "include/Compiler.h"
    "include/Evaluator.h"
    "include/KnownVal.h"
    "include/Lexer.h"
    "include/LiteralVal.h"
    "include/LlvmVal.h"
    "include/NamePool.h"
    "include/NodeVal.h"
    "include/Parser.h"
    "include/Processor.h"
    "include/Reserved.h"
    "include/StringPool.h"
    "include/SymbolTable.h"
    "include/Token.h"
    "include/TypeTable.h"
    "include/utils.h"
    )

set(SOURCE_FILES
    "src/ClangAdapter.cpp"
    "src/Codegen.cpp"
    "src/CompileMessages.cpp"
    "src/Compiler.cpp"
    "src/Evaluator.cpp"
    "src/KnownVal.cpp"
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

target_include_directories(orbc PRIVATE "include")

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
