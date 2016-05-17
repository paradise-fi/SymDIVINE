#include <llvmsym/programutils/config.h>

const char USAGE[] =
R"(SymDIVINE.

Usage:
  symdivine reachability <model> [options]
  symdivine ltl <property> <model> [options]
  symdivine (-h | --help | --version)

Options:
  -h --help               Show this screen.
  --version               Show version.
  --cheapsimplify         Use only cheap simplificatin methods.
  --dontsimplify          Disable simplification.
  --disabletimeout        Disable timeout for Z3.
  --iterative             Enables iterative depth search.
  -p --partialstore       Use partial SMT store (better caching).
  --testvalidity          When using partial store, compare results with full store.
  -c --enablecaching      Enable caching for Z3 formulas.
  -s --statistics         Enable output of statistics.
  --space_output=<file>   Outputs state space to <file> in dot format.
  --bound=<depth>         Limits depth exploration to given bound.
  -v --verbose            Enable verbose mode.
  -w --vverbose           Enable extended verbose mode.
)";

ConfigStruct Config;