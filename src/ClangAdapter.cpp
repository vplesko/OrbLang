#include "ClangAdapter.h"
#include <cstdio>
#include <vector>
#include "llvm/Support/Host.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "utils.h"
using namespace std;

bool buildExecutable(const ProgramArgs &args, const std::string &objFile) {
    string clangPath = llvm::sys::findProgramByName("clang").get();

    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpt(new clang::DiagnosticOptions());
    clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagId(new clang::DiagnosticIDs());
    clang::DiagnosticsEngine diags(diagId, diagOpt);

    clang::driver::Driver driver(clangPath, llvm::sys::getDefaultTargetTriple(), diags);

    vector<const char*> clangArgs;
    clangArgs.push_back(clangPath.c_str());
    if (!objFile.empty()) clangArgs.push_back(objFile.c_str());
    for (const string &in : args.inputsOther) clangArgs.push_back(in.c_str());
    clangArgs.push_back("-o");
    clangArgs.push_back(args.output.c_str());

    unique_ptr<clang::driver::Compilation> compilation(driver.BuildCompilation(clangArgs));
    if (compilation == nullptr) return false;
 
    clang::SmallVector<std::pair<int, const clang::driver::Command*>, 4> failingCommands;
    int res = driver.ExecuteCompilation(*compilation, failingCommands);
    if (res < 0) {
        for (const auto &p : failingCommands) {
            driver.generateCompilationDiagnostics(*compilation, *p.second);
        }
        return false;
    }

    return true;
}