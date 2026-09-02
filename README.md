# IPOPT runtime-DLL-loading issue (Windows)

This repository demonstrates a crash that occurs on Windows when IPOPT is loaded and unloaded from a DLL at runtime using `LoadLibrary`/`FreeLibrary` with the MUMPS linear solver.
The issue occurs when the DLL is repeatedly loaded and unloaded. 
Depending on the problem dimension, the program suddenly and deterministically exits with return code `3` during a call to `IpoptApplication::OptimizeTNLP`.

The crash appears to occur during the factorization step in the MUMPS linear solver, where `TerminateProcess` is called. 
One possible explanation is a memory-allocation failure that ultimately triggers a Fortran `STOP` statement, causing the process to terminate.

The problem used to reproduce the issue is a nonlinear programming problem obtained by transcribing the Van der Pol optimal-control problem. 
The problem is sufficiently large to trigger the crash, whereas standard IPOPT tutorial examples, such as HS071, are too small to reproduce it.

With the current settings, the crash occurs consistently after around 360 DLL load/unload cycles.

## Layout

```
ipopt-dll-example/
├── CMakeLists.txt          top-level: fetches IPOPT binaries, builds subprojects
├── solver_dll/
│   ├── CMakeLists.txt
│   └── src/
│       ├── vdp_ocp_nlp.hpp/.cpp   Ipopt::TNLP implementation (Van der Pol test problem)
│       └── solver_dll.cpp         extern "C" solve() entry point
└── app/
    ├── CMakeLists.txt
    └── main.cpp            loads/unloads solver_dll.dll N times
```

## Var den Pol optimal control problem

```
Direct-transcription (trapezoidal collocation) NLP for the Van der Pol
optimal control problem:

  minimize   integral_0^T ( x1^2 + x2^2 + u^2 ) dt
  subject to x1' = x2
             x2' = (1 - x1^2) * x2 - x1 + u
             x1(0) = x10, x2(0) = x20
             -1 <= u(t) <= 1

Discretized on N intervals (N+1 nodes) via trapezoidal collocation:

  x1_{k+1} - x1_k - dt/2 * ( x2_k + x2_{k+1} )                     = 0
  x2_{k+1} - x2_k - dt/2 * ( f2_k + f2_{k+1} )                     = 0

where f2_j = (1 - x1_j^2) * x2_j - x1_j + u_j.

Variable layout: for node k = 0..N, x[3k+0]=x1_k, x[3k+1]=x2_k, x[3k+2]=u_k
so n = 3*(N+1).
Constraint layout: for interval k = 0..N-1, row 2k = defect1_k,
row 2k+1 = defect2_k, so m = 2*N.
```

The problem is solved for `T=1`, `x10=0`, `x20=1`, and `N=100`.

## Building

Requires CMake 3.24+ and MSVC compiler (the fetched IPOPT binaries are built with `msvs2022-md`).

```bat
cmake -S . -B build
cmake --build build --config RelWithDebInfo
```

This will download the IPOPT release from GitHub the first time it configures (requires internet access), and produce, per configuration:

```
build/bin/app.exe
build/bin/solver_dll.dll
build/bin/ipopt.dll
```

## Running

```bat
build\bin\app.exe 1000
```

The argument is the number of times to load the DLL, solve, and unload it (default 1).

## Debugging

After running the program with a sufficiently high number of load/unload cycles (say 1000) you can attach a debugger to the process and set a breakpoint on `TerminateProcess` in `kernel32.dll` to catch the crash and then resume the process.
When the breakpoint is hit, you can inspect the call stack to see where the crash originated.

In the LLDB debugger, you can set the breakpoint with the following command:

```bat
breakpoint set --name TerminateProcess --shlib kernel32.dll
```

and inspect the call stack with:

```bat
bt
```

The expected output is something like

```
* thread #1, name = 'Main Thread', stop reason = breakpoint 1.1
  * frame #0: 0x00007ffd811f21b0 kernel32.dll`TerminateProcess
    frame #1: 0x00007ffbb1e94dfd coinmumps-3.dll
    frame #2: 0x00007ffbb1eac504 coinmumps-3.dll
    frame #3: 0x00007ffbb1e6121b coinmumps-3.dll
    [...]
    frame #18: 0x00007ffb98027022 ipopt-3.dll
    frame #19: 0x00007ffb97fa6fc1 ipopt-3.dll
    frame #20: 0x00007ffb97f96eff ipopt-3.dll
    frame #21: 0x00007ffb97e954fb ipopt-3.dll
    [...]
    frame #31: 0x00007ffd53f12d54 solver_dll.dll`solve() at solver_dll.cpp:20
    frame #32: 0x00007ff7e2d69279 app.exe`main(argc=<unavailable>, argv=<unavailable>) at main.cpp:64
```

where it is evident that `TerminateProcess` is called from `coinmumps-3.dll`, i.e. inside the linear solver MUMPS.

To further inspect where the issue originates, you can set the IPOPT option

```
mumps_print_level = 3
```

After running the program again, you will see a lot of MUMPS output in the console, and the last few lines before the crash will look like this:

```
****** FACTORIZATION STEP ********

 GLOBAL STATISTICS PRIOR NUMERICAL FACTORIZATION ...
 Number of working processes                =               1
 ICNTL(22) Out-of-core option               =               0
 ICNTL(35) BLR activation (eff. choice)     =               0
 ICNTL(37) BLR CB compression (eff. choice) =               0
 ICNTL(49) Compact workarray S (end facto.) =               0
 ICNTL(14) Memory relaxation                =            1000
 INFOG(3) Real space for factors (estimated)=            7147
 INFOG(4) Integer space for factors (estim.)=            2086
 Maximum frontal size (estimated)           =              15
 Number of nodes in the tree                =              43
 ICNTL(23) Memory allowed (value on host)   =               0
           Sum over all procs               =               0
 Memory provided by user, sum of LWK_USER   =               0
 Effective threshold for pivoting, CNTL(1)  =      0.1000D-05
 Effective size of S     (based on INFO(39))=               101147
 Elapsed time to reformat/distribute matrix =      0.0000


 Allocated buffers
 ------------------
 Size of reception buffer in bytes ...... =      2100000
 Size of async. emission buffer (bytes).. =      2100012
 Small emission buffer (bytes) .......... =           20
```

which shows that termination happens during the MUMPS factorization step.
The missing lines that appear in a normal MUMPS factorization step are the following:

```
** Memory allocated, total in Mbytes           (INFOG(19)):           5
** Memory effectively used, total in Mbytes    (INFOG(22)):           4
```

The lack of the above output probably indicates that MUMPS failed to allocate memory for the factorization.