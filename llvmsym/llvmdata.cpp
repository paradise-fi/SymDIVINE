#include <llvmsym/llvmdata.h>
#include <iostream>
#include <algorithm>

#include <llvmsym/llvmwrap/Constants.h>

namespace llvm_sym {

static const int PointerWidth = 64;

int getBitWidth( llvm::Type *t )
{
    if ( t->isVoidTy() )
        return 1;
    if ( t->isPointerTy() )
        return PointerWidth;

    assert( ( t->isIntegerTy()) );
    return t->getScalarSizeInBits();
}

int getElementsWidth( llvm::Type *t )
{
    assert( !t->isFunctionTy() );

    if ( t->isIntegerTy() || t->isVoidTy() || t->isPointerTy() )
        return 1;
    else if ( t->isStructTy() ) {
        int widths = 0;
        llvm::StructType *structty = llvm::cast< llvm::StructType >( t );

        auto element_iterator = structty->element_begin();
        for ( ; element_iterator != structty->element_end(); ++element_iterator ) {
            widths += getElementsWidth( *element_iterator );
        }

        return widths;
    } else if ( t->isVectorTy() ) {
        std::cerr << "vector types are not supported" << std::endl;
        abort();
    } else if ( t->isArrayTy() ) {
        llvm::ArrayType *arrty = llvm::cast< llvm::ArrayType >( t );
        return getElementsWidth( arrty->getElementType() ) * arrty->getNumElements();
    } else {
        std::cerr << "unknown type class, aborting." << std::endl;
        abort();
    }
}

namespace {
inline void helper_visit_and_collect_values(
        const llvm::Value *val,
        std::vector< const llvm::Value *> & values )
{
    values.push_back( val );
    const llvm::User *us_val = llvm::dyn_cast< llvm::User >( val );

    for ( int i = 0; us_val && i < us_val->getNumOperands(); ++i ) {
        if ( llvm::isa< llvm::ConstantExpr >( us_val->getOperand( i ) ) ) {
            helper_visit_and_collect_values(
                    llvm::cast< llvm::User >( us_val->getOperand( i ) ),
                    values );
        }
    }
}}

std::shared_ptr< const std::vector< const llvm::Value* > > collectUsedValues( const llvm::Function *fun )
{
    static std::map< const llvm::Function *,
        std::shared_ptr< const std::vector< const llvm::Value *> > > cache;

    auto found_it = cache.find( fun );
    if ( found_it != cache.end() )
        return found_it->second;

    std::vector< const llvm::Value* > values;

    for ( const auto &block : *fun ) {
        for ( auto i = block.begin(); i != block.end(); ++i ) {
            helper_visit_and_collect_values( i, values );
        }
    }

    for ( auto arg = fun->arg_begin(); arg != fun->arg_end(); ++arg ) {
        helper_visit_and_collect_values( arg, values );
    }

    cache[ fun ] = std::make_shared< std::vector< const llvm::Value * > >( values );
    return cache[ fun ];
}

std::vector< int > getPrimitiveTypeWidths( llvm::Type *t )
{
    assert( !t->isFunctionTy() );

    if ( t->isIntegerTy() || t->isVoidTy() || t->isPointerTy() ) {
        return std::vector< int >( 1, getBitWidth( t ) );
    } else if ( t->isStructTy() ) {
        std::vector< int > widths;
        llvm::StructType *structty = llvm::cast< llvm::StructType >( t );
        
        auto element_iterator = structty->element_begin();
        for ( ; element_iterator != structty->element_end(); ++element_iterator ) {
            std::vector< int > element_widths = getPrimitiveTypeWidths( *element_iterator );
            std::copy( element_widths.begin(), element_widths.end(), back_inserter( widths ) );
        }

        return widths;
    } else if ( t->isVectorTy() ) {
        std::cerr << "vector types are not supported" << std::endl;
        abort();
    } else if ( t->isArrayTy() ) {
        llvm::ArrayType *arrty = llvm::cast< llvm::ArrayType >( t );
        std::vector< int > widths;
        std::vector< int > element_widths = getPrimitiveTypeWidths( arrty->getElementType() );
        for ( unsigned i = 0; i < arrty->getNumElements(); ++i ) {
            std::copy(      element_widths.begin(),
                            element_widths.end(),
                            back_inserter( widths ) );
        }
        return widths;
    } else {
        std::cerr << "unknown type class, aborting." << std::endl;
        abort();
    }
}

}

