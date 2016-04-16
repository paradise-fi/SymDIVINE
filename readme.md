#SymDIVINE
SymDIVINE is a tool for control explicit/data symbolic bit-precise LTL verification of parallel C/C++ programs using LLVM bitcode as intermediate representation.

## How to run it
You can build SymDIVINE yourself (see section Building), or you can obtain pre-built binary for x64 Linux from [here](https://github.com/yaqwsx/SymDIVINE).

### Runtime dependencies
Runtime dependencies of SymDIVINE are: `libboost-graph1.54.0`, `z3`, `gcc-4.9` and `g++-4.9`. To compile C/C++ programs to LLVM you need `clang-3.5`. If you want to use LTL model checking also `ltl2tgba` is required.

On `apt-get` based system you can run following command to get all the dependencies: `sudo apt-get install libboost-graph1.54.0 z3` (tested on Ubuntu 15.10, on Ubuntu 14.04 you have tu build Z3 yourself and add respository `ppa:ubuntu-toolchain-r/test` and install newer version of g++).

If your system doesn't have package for Z3 SMT solver or you want to use the newest version (strongly recommended), you can download it or get sources and compile it from [their official repository](https://github.com/Z3Prover/z3).

### Running 
For simple assert reachability verification of LLVM bitcode run `bin/symdivine reachability <model.ll>` or see `bin/symdivne help` for more info. SymDIVINE supports pthreads, pthread mutexes, atomic sections via SV-COMP notation and more.

You can also use helper script `scripts/run_symdivine.py`, which takes a C a C++ file, compiles it and runs SymDIVINE.

## Building
To build SymDIVINE, yout need `cmake`, `make`, `llvm-3.5`, `z3`, `boost`, `flex` and `bison`.

Then just checkout this repo and run `./configure && cd build && make`. Final binary is `bin/symdivine`.

If you run to problems with `docopt`, run `git submodule update --init --recursive`.

## About the authors
Based on work by Vojtěch Havel and Peter Bauch in [ParaDiSe (Parallel & Distributed System Laboratory)](http://paradise.fi.muni.cz). Currently developed by Jan Mrázek and Henrich Lauko.
