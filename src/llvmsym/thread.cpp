#include <llvmsym/thread.h>
#include <ostream>

namespace llvm_sym {

std::ostream & operator<<( std::ostream &o, const Control &c )
{
    for ( unsigned tid = 0; tid < c.threadCount(); ++tid ) {
        o << "thread " << tid << ": ";
        
        for ( const Control::PC &p : c.context[tid] ) {
            o << "{ func: "
              << p.function << ", block: "
              << p.basicblock << ", inst: "
              << p.instruction << "}, ";
        }
        o << "\n";
    }


    return o;
}

}

