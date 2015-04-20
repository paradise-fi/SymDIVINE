#include <llvmsym/llvmwrap/Instructions.h>
#include <llvmsym/llvmwrap/Constants.h>

#include <llvmsym/stanalysis/inputvariables.h>

#include <llvmsym/cxa_abi/demangler.h>

namespace {
    bool _is_nonconstant_pointer( const llvm::Value *val )
    {
        return ( val->getType()->isPointerTy() && !llvm::isa< llvm::Constant >( val ) )
            || llvm::isa< llvm::GlobalVariable >( val );
    }
};

bool MultivalInfo::visit_value( const llvm::Value *val, const llvm::Function *parent )
{
    bool change = false;
    const llvm::User *v = llvm::dyn_cast< llvm::User >( val );

    if ( !v )
        return false;

    for ( unsigned i = 0; i < v->getNumOperands(); ++i ) {
        auto operand = v->getOperand( i );
        if ( llvm::isa< llvm::ConstantExpr >( operand ) ) {
            auto operand_expr = llvm::cast< llvm::ConstantExpr >( operand );
            change = change || visit_value( operand_expr, parent );
        }
    }

    if ( llvm::isa< llvm::Function >( val ) )
        return change;

    unsigned opcode;
    if ( llvm::isa< llvm::Instruction >( v ) ) {
        const llvm::Instruction *tmp_inst = llvm::cast< llvm::Instruction >( v );
        opcode = tmp_inst->getOpcode();
    } else {
        const llvm::ConstantExpr *tmp_inst = llvm::cast< llvm::ConstantExpr >( v );
        opcode = tmp_inst->getOpcode();
    }

    typedef llvm::Instruction LLVMInst;
    switch ( opcode ) {
        case LLVMInst::Ret: {
            const llvm::Value* returned = llvm::cast< llvm::ReturnInst >( v )->getReturnValue();
            if ( returned && isMultival( returned ) ) {
                if ( !isMultival( parent ) ) {
                    setMultival( parent );
                    change = true;
                }
            } else if ( returned && _is_nonconstant_pointer( returned ) ) {
                change = change || mergeRegionMultivalInfo( returned, parent );
            }
            return change;
        } case LLVMInst::GetElementPtr: {
            assert( v->getType()->isPointerTy() );
            const llvm::Value *elem = v->getOperand( 0 );
            const llvm::Type *elem_type = elem->getType()->getPointerElementType();
            std::vector< int > indices;
            if ( elem_type->isStructTy() ) {
                llvm::Type *ty = elem->getType()->getPointerElementType();
                for ( auto it = v->op_begin() + 2; ty->isStructTy() && it < v->op_end(); ++it ) {
                    assert( llvm::isa< llvm::ConstantInt >( it ) );
                    const llvm::ConstantInt *ci = llvm::cast< llvm::ConstantInt >( it );
                    int idx = ci->getLimitedValue();
                    indices.push_back( idx );
                    ty = ty->getContainedType( idx );
                }
            }
            return mergeRegionMultivalInfoSubtype( elem, v, indices );
        } case LLVMInst::ICmp:
          case LLVMInst::FCmp: {
            return change;
        } case LLVMInst::Store: {
            const llvm::Value *val_operand = llvm::cast< llvm::StoreInst >( v )->getValueOperand();
            const llvm::Value *ptr = llvm::cast< llvm::StoreInst >( v )->getPointerOperand();
            bool is_multival = isMultival( val_operand );

            if ( is_multival && !isRegionMultival( ptr ) ) {
                setRegionMultival( ptr );
                change = true;
            }

            if ( _is_nonconstant_pointer( val_operand ) ) {
                change = mergeRegionMultivalInfo( val_operand, ptr );
            }
            return change;
        } case LLVMInst::Load: {
            const llvm::Value *ptr = llvm::cast< llvm::LoadInst >( v )->getPointerOperand();
            if ( !v->getType()->isPointerTy() && !isMultival( v ) ) {
                if ( isRegionMultival( ptr ) ) {
                    setMultival( v );
                    change = true;
                }
            }

            if ( v->getType()->isPointerTy() ) {
                change = mergeRegionMultivalInfo( v, ptr );
            }
            return change;
        } case LLVMInst::Call: {
            const llvm::CallInst *ci = llvm::cast< llvm::CallInst >( v );
            const llvm::Value *called_value = ci->getCalledFunction() ?
                    ci->getCalledFunction()
                  : ci->getCalledValue();

            const llvm::Function *called_fun = ci->getCalledFunction();

            if ( isMultival( called_value ) && !isMultival( v ) ) {
                setMultival( v );
                change = true;
            } else if ( _is_nonconstant_pointer( v ) )
                change = change || mergeRegionMultivalInfo( v, called_value );

            if ( !called_fun )
                return change;

            auto arg = called_fun->arg_begin();
            for ( unsigned arg_no = 0; arg_no < ci->getNumArgOperands(); ++arg_no, ++arg) {
                const auto &operand = ci->getArgOperand( arg_no );
                if ( isMultival( operand ) ) {
                    if ( !isMultival( arg ) ) {
                        setMultival( arg );
                        change = true;
                    }
                }
                if ( _is_nonconstant_pointer( operand ) ) {
                    change = mergeRegionMultivalInfo( operand, arg );
                }
            }
            return change;
        } case LLVMInst::ExtractValue: {
            //TODO eventually
            abort();
        } case LLVMInst::InsertValue: {
            //TODO eventually
            abort();
        }
    }
    for ( unsigned operand = 0; operand < v->getNumOperands(); ++operand ) {
        if ( isMultival( v->getOperand( operand ) ) && !isMultival( v ) ) {
            setMultival( v );
            change = true;
            break;
        }
    }
    return change;

}

bool MultivalInfo::visit_function( const llvm::Function &function )
{
    bool change = false;

    for ( const llvm::Value *v : *llvm_sym::collectUsedValues( &function ) ) {
        change = change || visit_value( v, &function );
    }
    return change;
}

void MultivalInfo::get_multivalues( const llvm::Module *module )
{
    bool has_input = false;
    for ( const llvm::Function &f : *module ) {
        std::string fun_name = Demangler::demangle( f.getName() );
        if ( llvm_sym::isFunctionInput( fun_name ) ) {
            has_input = true;
            setMultival( &f );
        }
    }

    if ( !has_input )
        return;

    bool change = true;

    while ( change ) {
        change = false;
        for ( const llvm::Function &function : *module ) {
            change = change || visit_function( function );
        }
    }
}

