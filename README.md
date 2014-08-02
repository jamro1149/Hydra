# Hydra

Most computer programs are executing sequentially. This project, Hydra, designed and implemented automatic parallelisation of executable programs, a problem considered one of the ``holy grails'' in compiler research. Hydra inspects executable programs and identifies functions that can be parallelised, while assessing the potential gains. Using a cross-platform pool of processes, it then parallelises candidate functions and issues new compiled code that can be executed on multiple core or multiple CPU machines. Hydra achieves a 6.33$\times$ speedup of the classical $n$-body simulation problem when using a 48-core machine, without any user assistance. The project is also available online as open source.

## How to Compile and Run Hydra:

1) Checkout LLVM 3.4 from source and compile it (instructions @
http://llvm.org/docs/GettingStarted.html -- make sure you use /tags/RELEASE_34
to checkout 3.4 rather than SVN). If you intend on actually running the code
after Hydra has transformed it, you will also need Compiler-RT 3.4. Clang 3.4 is
useful for generating LLVM IR from C/C++/ObjC.

2) In Hydra's source directory, issue autoconf/AutoRegen.sh, which will ask for
your LLVM source and build directories. You will need to have autoconf installed
(use "sudo apt-get install autoconf" on Debian-like systems).

3) Make a build directory outside of Hydra's source tree
(use "mkdir ../build").

4) Whilst in the new build directory ("cd ../build"), run
"../configure --enablecxx11 --enable-optimized --disable-assertions". The first
option is absolutely required, but the other two can be dropped if you would
like the `default' build to contain debug symbols or assertions.

5) Compile the project with "make". You will need to have Make installed. In
addition, you *must* have a working C++11 compiler and standard library (the
configure stript from the previous step should have complained if not). Clang
3.4 with libstdc++ 3.8.1 is definately sufficient.

6) To run Hydra, use LLVM 3.4's opt tool, which you compiled in step 1. It is
vital that you use the same build options -- never try to load a Debug+Asserts
build of Hydra into a Release build of opt. Run opt with the -load options
directed to the required .so file (e.g. build/Release/lib/Analyses.so).

For instance, to run the decider on xxx.bc:
opt -load ~/proj-files/build/Release/lib/Analyses.so -analyze -decider xxx.bc

To transform xxx.bc into yyy.bc, use:
opt -load ~/proj-files/build/Release/lib/Analyses.so -load \
  ~/proj-files/build/Release/lib/Transforms.so -parallelisecalls xxx.bc \
  -o yyy.bc

Note that you must always load Analyses.so AND Transforms.so to successfully run
any of the two Transformation passes. Also, pass "-S" to opt to make it output
human-readable IR, rather than bitcode.

By default, Hydra will target the Thread Pool runtime. To target Kernel Threads,
you need to modify the include/hydra/Support/TargetMacros.h file and recompile
("make" in build directory).

To execute a transformed file yyy.bc, the easiest way is to use Clang that you
build in step 1 with the "-pthread" option. If yyy targets the Thread Pool, that
needs to be compiled too, with -DNUM_THREADS=xx, where xx is the number of
worker threads you want. E.g. for 3 worker threads use:

clang++ -pthread -DNUM_THREADS=3 yyy.bc threading/ThreadPool.cpp -O3

I recommend that you compile with at least O2, since the Analyser relies on an
optimised pool. If yyy.bc targets Kernel Threads, you can omit the
ThreadPool.cpp and the NUM_THREADS macro.
