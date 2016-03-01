#pragma once

#include <llvmsym/llvmwrap/Instructions.h>
#include <llvmsym/llvmwrap/Module.h>
#include "llvmsym/programutils/statistics.h"
#include "llvmsym/datastore.h"

template < typename Evaluator >
class Dispatcher {
    const Evaluator &self() const
    {
        return *static_cast< const Evaluator* >( this );
    }
    
    Evaluator &self()
    {
        return *static_cast< Evaluator* >( this );
    }
    
    typedef llvm_sym::DataStore::Value Value;

    public:

    template < typename Yield >
    void execute( llvm::User *inst, int tid, Yield yield )
    {
        for ( unsigned arg = 0; arg < inst->getNumOperands(); ++arg ) {
            llvm::Value *operand = inst->getOperand( arg );
            if ( llvm::isa< llvm::ConstantExpr >( operand ) ) {
                execute( llvm::cast< llvm::ConstantExpr >( operand ), tid, yield );
            }
        }
        ++Statistics::getCounter( STAT_INSTR_EXECUTED );

        assert( llvm::isa< llvm::Instruction >( inst ) || llvm::isa< llvm::ConstantExpr >( inst ) );

        unsigned opcode;
        std::string opcodename;
        bool is_constexpr = llvm::isa< llvm::ConstantExpr >( inst );

        if ( is_constexpr ) {
            const llvm::ConstantExpr * tmp_inst = llvm::cast< llvm::ConstantExpr >( inst );
            opcode = tmp_inst->getOpcode();
            opcodename = tmp_inst->getOpcodeName();
        } else {
            const llvm::Instruction *tmp_inst = llvm::cast< llvm::Instruction >( inst );
            opcode = tmp_inst->getOpcode();
            opcodename = tmp_inst->getOpcodeName();
        }

        switch ( opcode ) {
            case llvm::Instruction::ICmp:
                self().do_icmp( llvm::cast< llvm::ICmpInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::PHI:
                self().do_phi( llvm::cast< llvm::PHINode >( inst ), tid, yield );
                break;
            case llvm::Instruction::ZExt:
            case llvm::Instruction::FPExt:
            case llvm::Instruction::SExt:
            case llvm::Instruction::FPTrunc:
            case llvm::Instruction::Trunc:
                self().do_cast( llvm::cast< llvm::CastInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Br:
                self().do_break( llvm::cast< llvm::BranchInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Switch:
                self().do_switch( llvm::cast< llvm::SwitchInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Call:
                self().do_call( llvm::cast< llvm::CallInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Ret:
                self().do_return( llvm::cast< llvm::ReturnInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Load:
                self().do_load( llvm::cast< llvm::LoadInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Store:
                self().do_store( llvm::cast< llvm::StoreInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::Alloca:
                self().do_alloca( llvm::cast< llvm::AllocaInst >( inst ), tid, yield );
                break;

            /* binary arithmetics */
            case llvm::Instruction::FDiv:
            case llvm::Instruction::SDiv:
            case llvm::Instruction::UDiv:
            case llvm::Instruction::FMul:
            case llvm::Instruction::Mul:
            case llvm::Instruction::FAdd:
            case llvm::Instruction::FSub:
            case llvm::Instruction::Sub:
            case llvm::Instruction::FRem:
            case llvm::Instruction::URem:
            case llvm::Instruction::SRem:
            case llvm::Instruction::And:
            case llvm::Instruction::Or:
            case llvm::Instruction::Xor:
            case llvm::Instruction::Shl:
            case llvm::Instruction::LShr:
            case llvm::Instruction::Add: {
              // std::cout << "LLVMari" << std::endl;
                Value res = self().deref( llvm::cast< llvm::Value >( inst ), tid, false );
                Value a = self().deref( inst->getOperand( 0 ), tid );
                Value b = self().deref( inst->getOperand( 1 ), tid );
                

                bool multival = self().state.layout.isMultival( a ) || self().state.layout.isMultival( b );
                if ( multival )
                    self().do_binary_arithmetic_op( opcode, self().state.data, res, a, b, tid );
                else
                    self().do_binary_arithmetic_op( opcode, self().state.explicitData, res, a, b, tid );

                if ( !is_constexpr )
                    yield( false, false, true );
                break;
            }

            case llvm::Instruction::GetElementPtr:
                self().do_GEP( inst, tid );
                if ( !is_constexpr )
                    yield( false, false, true );
                break;

            case llvm::Instruction::BitCast:
            case llvm::Instruction::Unreachable:
                // noop
                if ( !is_constexpr )
                    yield( false, false, true );
                break;
            case llvm::Instruction::Select:
                self().do_ite( llvm::cast< llvm::SelectInst >( inst ), tid, yield );
                break;
            case llvm::Instruction::PtrToInt: {
                llvm::Value* res = llvm::cast<llvm::Value>(inst);
                llvm::Value* oper = inst->getOperand(0);
                self().do_ptrtoint(res, oper, tid);
                if (!is_constexpr) {
                    yield(false, false, false);
                }
            }
                break;
            case llvm::Instruction::IntToPtr: {
                llvm::Value* res = llvm::cast<llvm::Value>(inst);
                llvm::Value* oper = inst->getOperand(0);
                self().do_inttoptr(res, oper, tid);
                if (!is_constexpr) {
                    yield(false, false, false);
                }
            }
                break;
            case llvm::Instruction::LandingPad:
            case llvm::Instruction::Fence:
            case llvm::Instruction::SIToFP:
            case llvm::Instruction::FPToSI:
            case llvm::Instruction::AtomicCmpXchg:
            case llvm::Instruction::AtomicRMW:
            case llvm::Instruction::ExtractValue:
            case llvm::Instruction::InsertValue:
            case llvm::Instruction::AShr:
            case llvm::Instruction::IndirectBr:
            case llvm::Instruction::Resume:
            case llvm::Instruction::Invoke:
            case llvm::Instruction::UIToFP:
            case llvm::Instruction::FPToUI:
            case llvm::Instruction::FCmp:
            default: {
                std::cerr << "unknown instruction " << opcodename << std::endl;
                inst->dump();
                auto i = llvm::cast<llvm::Instruction>(inst);
                std::cerr << i->getParent()->getParent()->getName().str();
                abort();
                break;
            }
        }
    
    }
};
