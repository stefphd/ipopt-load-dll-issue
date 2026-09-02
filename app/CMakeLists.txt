add_executable(app main.cpp)

# app.exe does NOT link against solver_dll or ipopt: it loads solver_dll.dll
# purely at runtime via LoadLibrary/GetProcAddress. add_dependencies() only
# guarantees build ORDER (solver_dll must exist before app is run), not a
# link-time dependency.
add_dependencies(app solver_dll)
