#include <iostream>

#include <llvmsym/error.h>

namespace llvm_sym {

void die( ErrorCause cause )
{
    switch ( cause ) {
        case ErrorCause::SYMBOLIC_POINTER:
            std::cerr << "Symbolic pointers are not implemented yet." << std::endl;
            abort();
        case ErrorCause::SYMBOLIC_SELECT:
            std::cerr << "Select on symbolic condition is not implemented yet." << std::endl;
            abort();
        case ErrorCause::VARIABLE_LEN_ARRAY:
            std::cerr << "Variable-length arrays are not implemented yet." << std::endl;
            abort();
    }
}
}


