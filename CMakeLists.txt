cmake_minimum_required(VERSION 3.4.3)

project(OrbCompiler VERSION 0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ORBC_LIBS_SUBDIR lib/Orb)
set(ORBC_LIBS_PATH "${CMAKE_INSTALL_PREFIX}/${ORBC_LIBS_SUBDIR}" CACHE PATH "Path to Orb libraries directory.")

if(WIN32)
    set(PLATFORM_WINDOWS true)
else()
    set(PLATFORM_WINDOWS false)
endif()

if(UNIX)
    set(PLATFORM_UNIX true)
else()
    set(PLATFORM_UNIX false)
endif()

configure_file("src/OrbCompilerConfig.h.in" OrbCompilerConfig.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

set(HEADER_FILES
    "src/AttrMap.h"
    "src/BlockControl.h"
    "src/ClangAdapter.h"
    "src/CodeLoc.h"
    "src/CompilationOrchestrator.h"
    "src/CompilationMessages.h"
    "src/Compiler.h"
    "src/Evaluator.h"
    "src/EvalVal.h"
    "src/EscapeScore.h"
    "src/exceptions.h"
    "src/Lexer.h"
    "src/LifetimeInfo.h"
    "src/LiteralVal.h"
    "src/LlvmVal.h"
    "src/NamePool.h"
    "src/NodeVal.h"
    "src/Parser.h"
    "src/Processor.h"
    "src/ProgramArgs.h"
    "src/Reserved.h"
    "src/SpecialVal.h"
    "src/StringPool.h"
    "src/SymbolTable.h"
    "src/SymbolTableIds.h"
    "src/Token.h"
    "src/TypeTable.h"
    "src/UndecidedCallableVal.h"
    "src/utils.h"
    )

set(SOURCE_FILES
    "src/AttrMap.cpp"
    "src/BlockControl.cpp"
    "src/ClangAdapter.cpp"
    "src/CompilationOrchestrator.cpp"
    "src/CompilationMessages.cpp"
    "src/Compiler.cpp"
    "src/Evaluator.cpp"
    "src/EvalVal.cpp"
    "src/Lexer.cpp"
    "src/LifetimeInfo.cpp"
    "src/LiteralVal.cpp"
    "src/LlvmVal.cpp"
    "src/main.cpp"
    "src/NamePool.cpp"
    "src/NodeVal.cpp"
    "src/Parser.cpp"
    "src/Processor.cpp"
    "src/ProgramArgs.cpp"
    "src/Reserved.cpp"
    "src/SpecialVal.cpp"
    "src/StringPool.cpp"
    "src/SymbolTable.cpp"
    "src/SymbolTableIds.cpp"
    "src/Token.cpp"
    "src/TypeTable.cpp"
    "src/UndecidedCallableVal.cpp"
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

if(WIN32)
	target_link_libraries(orbc ${LLVM_LIBS})
else()
	target_link_libraries(orbc stdc++fs)
	target_link_libraries(orbc LLVM)
endif()

target_link_libraries(orbc clangTooling)

# install targets
install(TARGETS orbc DESTINATION bin)
install(DIRECTORY libs/ DESTINATION ${ORBC_LIBS_SUBDIR})
