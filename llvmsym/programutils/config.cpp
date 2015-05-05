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
      --dontsimplify        Disables simplification
      --disabletimout       Disables timeout for Z3
      -c --enablecaching    Enables caching of Z3 formulas
      -s --statistics       Enables output of statistics
      -v --verbose          Enables verbose mode
      -w --vverbose         Enables extended verbose mode
)";

ConfigStruct Config;