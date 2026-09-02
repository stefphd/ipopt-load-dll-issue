add_library(solver_dll SHARED
    src/solver_dll.cpp
    src/vdp_ocp_nlp.cpp
)

target_include_directories(solver_dll PRIVATE include)
target_compile_definitions(solver_dll PRIVATE SOLVER_DLL_EXPORTS)
target_link_libraries(solver_dll PRIVATE ipopt)

# ipopt.dll is not on PATH, so copy it next to solver_dll.dll after every
# build. Windows resolves a DLL's own dependencies by (among other places)
# searching the directory the DLL itself lives in, so this lets
# LoadLibrary("solver_dll.dll") succeed without touching the system PATH.
add_custom_command(TARGET solver_dll POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ipopt_LIBDIR}
    "$<TARGET_FILE_DIR:solver_dll>"
    COMMENT "Copying ipopt.dll next to solver_dll.dll"
)