#SymDIVINE
Is a tool for symbolic LTL verification of parallel C/C++ programs. It is based on verification of LLVM bitcode.

## Building
To build SymDIVINE, yout need `cmake`, `make`, `llvm-3.5`, `z3`, `boost`, `flex` and `bison`.

Then just checkout this repo and run `./configure && cd build && cmake && make`. Final binary is `bin/symdivine`.

## How to run it
See `bin/symdivne help` or use helper script `run_benchmark.py`

## About authors
Based on work by Vojtěch Havel and Peter Bauch in ["ParaDiSe (Parallel & Distributed System Laboratory)"](http://paradise.fi.muni.cz )
