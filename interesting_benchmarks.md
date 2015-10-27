# List of interesting tests

This document contains all the interesting, weird, suspicious and others benchmarks in context SymDIVINE.

## SV-COMP benchmarks

These tests are marked as false by SymDIVINE, however they are marked as true:

- `pthread-ext/11_fmaxsymopt_true-unreach-call.i`- ERROR: uncaught exception: bit-vector size must be greater than zero
- `pthread-ext/10_fmaxsym_cas_true-unreach-call.i`- ERROR: uncaught exception: Sorts (_ BitVec 32) and (_ BitVec 1) are incompatible
- `pthread/indexer_true-unreach-call.i` - ERROR: uncaught exception: Sorts (_ BitVec 32) and (_ BitVec 127) are incompatible