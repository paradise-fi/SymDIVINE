#include <llvmsym/emptydatastore.h>

namespace llvm_sym {
  
std::ostream & operator<<( std::ostream & o, const EmptyStore &v ) {
  o << "data:\n";        
  return o;
}

}
