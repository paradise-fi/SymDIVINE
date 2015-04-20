#pragma once
#include <llvmsym/llvmwrap/Module.h>
#include <llvmsym/llvmwrap/Instructions.h>
#include <vector>

namespace llvm_sym {

struct BB {
    llvm::BasicBlock *bb;
    std::vector< llvm::Instruction* > body;
    int stack_offset;

    explicit BB( llvm::BasicBlock* b, int of ): bb( b ), stack_offset( of )
    {
        assert( bb );
        llvm::BasicBlock::iterator inst_iter;
        for ( inst_iter = bb->begin(); inst_iter != bb->end(); ++inst_iter ) {
            body.push_back( inst_iter );
        }
    }
};

int getBitWidth( llvm::Type *t );
std::vector< int > getPrimitiveTypeWidths( llvm::Type *t );

// width of type, measured in primitive elements
int getElementsWidth( llvm::Type *t );

std::shared_ptr< const std::vector< const llvm::Value* > >
    collectUsedValues( const llvm::Function *fun );

inline bool isFunctionAssume( const std::string name )
{
    if ( name == "assume" )
        return true;
    if ( name == "__VERIFIER_assume" )
        return true;

    return false;
}

inline bool isFunctionInput( const std::string name )
{
    if ( name == "input" )
        return true;
    if ( name == "nondet_int" )
        return true;

    const std::string verifier_nondet_prefix = "__VERIFIER_nondet_";

    if ( name.compare( 0, verifier_nondet_prefix.size(), verifier_nondet_prefix ) == 0 )
        return true;

    return false;
}

struct Function {
    llvm::Function *llvm_fun;
    std::vector< BB > body;

    explicit Function( llvm::Function* f ) : llvm_fun( f )
    {
        llvm::Function::iterator bb_iter;
        int stack_offset = 0;
        for ( bb_iter = f->begin(); bb_iter != f->end(); ++bb_iter ) {
            body.push_back( BB( bb_iter, stack_offset ) );
            stack_offset += body.back().body.size();
        }
    }
};

}

