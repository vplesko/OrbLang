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

bool buildExecutable(const string &objFile, const string &exeName) {
    string clangPath = llvm::sys::findProgramByName("clang").get();

    clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpt(new clang::DiagnosticOptions());
    clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagId(new clang::DiagnosticIDs());
    clang::DiagnosticsEngine diags(diagId, diagOpt);

    clang::driver::Driver driver(clangPath, llvm::sys::getDefaultTargetTriple(), diags);

    vector<const char*> args;
    args.push_back(clangPath.c_str());
    args.push_back(objFile.c_str());
    args.push_back("-o");
    args.push_back(exeName.c_str());

    unique_ptr<clang::driver::Compilation> compilation(driver.BuildCompilation(args));
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