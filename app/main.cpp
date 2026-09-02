// Loads solver_dll.dll at runtime with LoadLibrary(), resolves its `solve`
// export with GetProcAddress(), calls it, then unloads the DLL with
// FreeLibrary() -- repeated N times. Note that this executable does NOT
// link against solver_dll.lib or include solver_api.h/IPOPT headers: the
// only thing it needs to know is the exported function's signature.

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#undef max

namespace fs = std::filesystem;

// Must match the signature of `solve` exported by solver_dll.dll
// (see solver_dll/include/solver_api.h).
using SolveFunc = int (*)();

// Returns the directory containing the running executable, so solver_dll.dll
// (built into the same output directory) can be found regardless of the
// process's current working directory.
static fs::path get_executable_dir() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return fs::current_path();
    }
    return fs::path(buffer).parent_path();
}

int main(int argc, char** argv) {
    int numIterations = 1;
    if (argc > 1) {
        numIterations = std::max(1, std::atoi(argv[1]));
    }

    const fs::path dllPath = get_executable_dir() / "solver_dll.dll";
    std::cout << "Loading solver DLL from: " << dllPath.string() << "\n\n";

    for (int i = 0; i < numIterations; ++i) {
        std::cout << "=== Run " << (i + 1) << " / " << numIterations << " ===\n";

        // --- Load the DLL ---
        const HMODULE hModule = LoadLibrary(dllPath.string().c_str());
        if (hModule == nullptr) {
            std::cerr << "  Failed to load DLL, error " << GetLastError() << "\n";
            return EXIT_FAILURE;
        } else {
            std::cout << "  DLL loaded.\n";
        }

        // --- Resolve the exported solve() function ---
        const auto solveFn = reinterpret_cast<SolveFunc>(GetProcAddress(hModule, "solve"));
        if (solveFn == nullptr) {
            std::cerr << "  Failed to find 'solve' export, error " << GetLastError() << "\n";
            FreeLibrary(hModule);
            return EXIT_FAILURE;
        }

        // --- Call it ---
        int status = solveFn();

        // --- Unload the DLL ---
        if (!FreeLibrary(hModule)) {
            std::cerr << "  Failed to unload DLL, error " << GetLastError() << "\n";
        } else {
            std::cout << "  DLL unloaded.\n";
        }
        std::cout << "\n";
    }

    return EXIT_SUCCESS;
}
