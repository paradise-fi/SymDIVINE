#include <llvmsym/datastore.h>

namespace llvm_sym {

ICmp_Op icmp_negate( ICmp_Op what )
{
    ICmp_Op retVal;
    switch ( what ) {
        case ICmp_Op::EQ:
            retVal = ICmp_Op::NE;
            break;
        case ICmp_Op::NE:
            retVal = ICmp_Op::EQ;
            break;
        case ICmp_Op::UGT:
            retVal = ICmp_Op::ULE;
            break;
        case ICmp_Op::UGE:
            retVal = ICmp_Op::ULT;
            break;
        case ICmp_Op::ULT:
            retVal = ICmp_Op::UGE;
            break;
        case ICmp_Op::ULE:
            retVal = ICmp_Op::UGT;
            break;
        case ICmp_Op::SGT:
            retVal = ICmp_Op::SLE;
            break;
        case ICmp_Op::SGE:
            retVal = ICmp_Op::SLT;
            break;
        case ICmp_Op::SLT:
            retVal = ICmp_Op::SGE;
            break;
        case ICmp_Op::SLE:
            retVal = ICmp_Op::SGT;
            break;
        default:
            retVal = ICmp_Op::SLE; // Suppress compiler warning
            assert(false);
    }
    return retVal;
}

}

