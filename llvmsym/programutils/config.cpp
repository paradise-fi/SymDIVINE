#include <llvmsym/programutils/config.h>

const char USAGE[] =
R"(SymDiVine.
    Usage:
      symdivine reachability [options] <model>
      symdivine ltl [options] <property> <model>
      symdivine (-h | --help)
      symdivine --version
    Options:
      -h --help             Show this screen.
      --version             Show version.
      --cheapsimplify       Use only cheap simplification methods
      --dontsimplify        Disable simplification
      --disabletimeout      Disable timeout for Z3
      -p --partialstore     Use partial SMT store (better caching)
      -c --enablecaching    Enable caching of Z3 formulas
      -s --statistics       Enable output of statistics
      -v --verbose          Enable verbose mode
      -w --vverbose         Enable extended verbose mode
)";

ConfigStruct Config;