// Loads solver_dll.dll at runtime with LoadLibrary(), resolves its `solve`
// export with GetProcAddress(), calls it, then unloads the DLL with
// FreeLibrary() -- repeated N times. Note that this executable does NOT
// link against solver_dll.lib or include solver_api.h/IPOPT headers: the
// only thing it needs to know is the exported function's signature.

#define HOOK_TERMINATE_PROCESS

#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#undef max

#ifdef HOOK_TERMINATE_PROCESS
#   include <dbghelp.h>
#   pragma comment(lib, "dbghelp.lib")
#endif

namespace fs = std::filesystem;

using SolveFunc = int (*)();

static fs::path get_executable_dir() {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        return fs::current_path();
    }
    return fs::path(buffer).parent_path();
}

#ifdef HOOK_TERMINATE_PROCESS
typedef BOOL (WINAPI *TerminateProcess_t)(HANDLE, UINT);
static TerminateProcess_t g_realTerminateProcess = nullptr;

BOOL WINAPI MyTerminateProcess(HANDLE hProcess, UINT uExitCode) {
    fprintf(stderr, "[HOOK] TerminateProcess(exitCode=%u) called!\n", uExitCode);
    // Capture and print a stack trace
    void* stack[64];
    USHORT frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    for (USHORT i = 0; i < frames; i++) {
        DWORD64 addr = (DWORD64)(stack[i]);
        char buffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)buffer;
        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        if (SymFromAddr(GetCurrentProcess(), addr, 0, symbol)) {
            fprintf(stderr, "  [%d] %s - 0x%0llX\n", i, symbol->Name, symbol->Address);
        }
        else {
            fprintf(stderr, "  [%d] 0x%0llX\n", i, addr);
        }
    }
    return g_realTerminateProcess(hProcess, uExitCode);
}

void HookTerminateProcessInModule(const char* moduleName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return;

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    g_realTerminateProcess = (TerminateProcess_t)GetProcAddress(hKernel32, "TerminateProcess");

    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dosHeader->e_lfanew);
    PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hMod +
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    for (; importDesc->Name != 0; importDesc++) {
        const char* name = (const char*)((BYTE*)hMod + importDesc->Name);
        if (_stricmp(name, "kernel32.dll") != 0) {
            continue;
        }

        PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)((BYTE*)hMod + importDesc->FirstThunk);
        PIMAGE_THUNK_DATA origThunk = (PIMAGE_THUNK_DATA)((BYTE*)hMod + importDesc->OriginalFirstThunk);

        for (; origThunk->u1.Function != 0; origThunk++, thunk++) {
            if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) continue;
            PIMAGE_IMPORT_BY_NAME funcName = (PIMAGE_IMPORT_BY_NAME)((BYTE*)hMod + origThunk->u1.AddressOfData);
            if (strcmp((char*)funcName->Name, "TerminateProcess") == 0) {
                DWORD oldProtect;
                VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect);
                thunk->u1.Function = (ULONG_PTR)MyTerminateProcess;
                VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                return;
            }
        }
    }
}
#endif

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

#ifdef HOOK_TERMINATE_PROCESS
        HookTerminateProcessInModule("coinmumps-3.dll");
#endif

        // --- Resolve the exported solve() function ---
        const auto solveFn = reinterpret_cast<SolveFunc>(GetProcAddress(hModule, "solve"));
        if (solveFn == nullptr) {
            std::cerr << "  Failed to find 'solve' export, error " << GetLastError() << "\n";
            FreeLibrary(hModule);
            return EXIT_FAILURE;
        }

        // --- Call it ---
        solveFn();

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
