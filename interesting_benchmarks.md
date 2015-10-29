# List of interesting tests

This document contains all the interesting, weird, suspicious and others benchmarks in context SymDIVINE.

## SV-COMP benchmarks

These tests are marked as false by SymDIVINE, however they are marked as true:

- `pthread-ext/11_fmaxsymopt_true-unreach-call.i`- variable offset in GEP, expecting constant
- `pthread-ext/10_fmaxsym_cas_true-unreach-call.i`- variable offset in GEP, expecting constant
- `pthread/indexer_true-unreach-call.i` - variable in load, expecting constant
- `pthread-lit/sssc12_true-unreach-call.i` - missing malloc support