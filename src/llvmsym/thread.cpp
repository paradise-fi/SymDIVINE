#include <llvmsym/thread.h>
#include <ostream>

namespace llvm_sym {

std::ostream & operator<<( std::ostream &o, const Control &c )
{
    for ( unsigned tid = 0; tid < c.threadCount(); ++tid ) {
        o << "thread " << tid << '\n';
        
        for ( const Control::PC &p : c.context[tid] ) {
            o << '{'
              << p.function << ", "
              << p.basicblock << ", "
              << p.instruction << "}\n";
        }
    }


    return o;
}

}

