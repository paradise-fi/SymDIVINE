#pragma once

#include <stack>
#include <sstream>
#include <iostream>

#include <llvmsym/thread.h>
#include <llvmsym/properties.h>
#include <llvmsym/llvmdata.h>
#include <llvmsym/memorylayout.h>
#include <llvmsym/explicitstore.h>

#include <llvmsym/programutils/statistics.h>
#include <llvmsym/programutils/config.h>
#include <llvmsym/cxa_abi/demangler.h>
#include <llvmsym/error.h>

#include <llvmsym/llvmwrap/Module.h>
#include <llvmsym/llvmwrap/Instructions.h>
#include <llvmsym/llvmwrap/LLVMContext.h>
#include <llvmsym/llvmwrap/IRReader.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/SourceMgr.h>

#define STAT_INSTR_EXECUTED "Instruction executed"
#define STAT_INSTR_OBSERVABLE_EXECUTED "Instructions executed observable"

#include <llvmsym/instructiondispatch.h>

namespace llvm_sym {

struct BitCode {
    std::shared_ptr< llvm::LLVMContext > ctx;
    std::shared_ptr< llvm::Module > module;

    BitCode( std::string file )
    {
        llvm::SMDiagnostic diag;
        ctx = std::make_shared< llvm::LLVMContext >();
        std::string err;
        module.reset( llvm::ParseIRFile( file, diag, *ctx ) );
    
        if ( !module ) {
            llvm::raw_os_ostream o( std::cerr );
            diag.print( file.c_str(), o );
            o.flush();
            exit( 1 );
        }
    }
};

// YieldEffect basically does sequencing of function calls.

template < typename Yield, typename Effect >
struct YieldEffect {
    const Yield &yield;
    const Effect &effect;

    template < typename ... Params >
    void operator() ( Params ... p ) const
    {
        effect();
        yield( p ... );
    }

    YieldEffect( const Yield &y, const Effect &r ) : yield( y ), effect( r )
    {
    }
};

template < typename Yield, typename Effect >
YieldEffect< Yield, Effect > makeYieldEffect( const Yield &yield, const Effect &effect )
{
    return YieldEffect< Yield, Effect >( yield, effect );
}

template < typename DataStore >
class Evaluator : Dispatcher< Evaluator< DataStore > >{
    typedef Control::PC PC;
    typedef typename DataStore::Value Value;

    typedef ExplicitStore::Pointer Pointer;
    typedef Dispatcher< Evaluator< DataStore > > InstDispatch;

    struct State {
        Control control;
        DataStore data;
        MemoryLayout layout;
        ExplicitStore explicitData;
        Properties properties;

        size_t getSize() const
        {
            return control.getSize() + data.getSize()
                + explicitData.getSize() + layout.getSize() + properties.getSize();
        }

        State( const llvm::Module *m ) : layout( m ) {}

        bool symEquivalent( const State &snd )
        {
            return data.equal( snd.data );
        }
    };

    llvm::Function *main;
    State state;
    std::shared_ptr< BitCode > bc;

    std::map< const llvm::Function *, int > functionmap;
    std::map< const llvm::BasicBlock *, PC > blockmap;
    std::vector< Function > functions;

    const std::set< std::string > ignored_functions = {
        "llvm.lifetime.start",
        "llvm.lifetime.end",
        "printf",
        "puts",
        "fprintf",
        "pthread_mutex_destroy",
        "pthread_mutex_init",
        "llvm.stacksave",
        "llvm.stackrestore",
    };

    std::vector< int > getBitWidthList( int fun_id ) const {
        std::vector< int > result;
        auto values = collectUsedValues( functions[ fun_id ].llvm_fun );

        for ( const llvm::Value *v : *values ) {
            std::vector< int > instr_widths = getPrimitiveTypeWidths( v->getType() );
            assert( instr_widths.size() == 1 ); // just for now

            std::copy( instr_widths.begin(), instr_widths.end(), back_inserter( result ) );
        }
        return result;
    }

    void getGlobalsBitWidths( std::vector< int > &widths ) const {
        for ( auto g = bc->module->getGlobalList().begin(); g != bc->module->getGlobalList().end(); ++g ) {
            std::vector< int > instr_widths = getPrimitiveTypeWidths( g->getType()->getPointerElementType() );

            std::copy( instr_widths.begin(), instr_widths.end(), back_inserter( widths ) );
        }
    }

    std::vector< int > getGlobalsWidths( ) const {
        std::vector< int > widths;
        for ( auto g = bc->module->getGlobalList().begin(); g != bc->module->getGlobalList().end(); ++g ) {
            assert( g->getType()->isPointerTy() );
            auto type = g->getType()->getPointerElementType();

            int width = getElementsWidth( type );
            widths.push_back( width );
        }
        return widths;
    }

    const BB getBB( const PC pc ) const
    {
        return getBB( pc.function, pc.basicblock );
    }

    const BB getBB( int function, int basicblock ) const
    {
        return functions[ function ].body[ basicblock ];
    }
    
    llvm::Instruction *fetchPC( int tid, const PC thread_pc ) const
    {
        assert( thread_pc.function < functions.size() );
        const Function &f = functions[ thread_pc.function ];

        assert( thread_pc.basicblock < f.body.size() );
        const BB &block = f.body[ thread_pc.basicblock ];

        assert( thread_pc.instruction < block.body.size() );
       
        assert( block.body[ thread_pc.instruction] );
        return block.body[ thread_pc.instruction ];
    
    }

    Value deref( Pointer ptr ) const
    {
        Value ret;
        ret.type = Value::Type::Variable;
        ret.variable.segmentId = ptr.content.segmentId;
        ret.variable.offset = ptr.content.offset;

        return ret;
    }

    Value deref( const llvm::Value *v, int tid, bool expl2const = true, bool prev = false ) const
    {
        auto res = state.layout.deref( v, tid, prev );
        int bw = getBitWidth( v->getType() );

        if ( expl2const && !state.layout.isMultival( res ) )
            return Value( state.explicitData.get( res ), bw, v->getType()->isPointerTy() );
        else
            return res;
    }

    llvm::Instruction *fetch( int tid ) const
    {
        const PC thread_pc = state.control.getPC( tid );
        return fetchPC( tid, thread_pc );
    }

    void jumpTo( const llvm::BasicBlock *bb, int tid )
    {
        const PC &bb_pc = blockmap[ bb ];
        state.control.jumpTo( bb_pc, tid, hasPHI( bb ) );
        state.layout.switchBB( getBB( bb_pc ), tid );
    }

    bool hasPHI( const llvm::BasicBlock *bb ) const
    {
        const llvm::Instruction *first_instr = bb->begin();
        return llvm::isa< llvm::PHINode >( first_instr );
    }

    bool amILonelyThread() const
    {
        return state.control.threadCount() == 1;
    }

    void leave( int tid )
    {
        bool last = state.control.last( tid );
        // restore previous PC or leave thread
        state.control.leave( tid );
        
        // drop stack
        std::pair< int, int > segments_range = state.layout.getLastStackSegmentRange( tid );

        for ( int s = segments_range.first; s < segments_range.second; ++s ) {
            state.data.eraseSegment( segments_range.first );
            state.explicitData.eraseSegment( segments_range.first );
            state.layout.eraseSegment( segments_range.first );
        }
        state.explicitData.movePointers(
                segments_range.first,
                -( segments_range.second - segments_range.first ) );
        state.layout.leave( tid );

        // if not returning from last function, set block stack memory layout
        if ( !last ) {
            const PC &act_pc = state.control.getPC( tid );
            state.layout.switchBB( getBB( act_pc ), tid );
        }
    }

    void enterFunction( short unsigned fun_id, int tid )
    {
        state.control.enterFunction( fun_id, tid );
        state.layout.newStack( tid );
        state.layout.newSegment( tid );
        int sid = state.layout.getLastStackSegmentRange( tid ).first;

        const std::vector< int > &bitwidths = getBitWidthList( fun_id );

        state.data.addSegment( sid, bitwidths );
        state.explicitData.addSegment( sid, bitwidths );
        state.layout.addSegment( sid, bitwidths );

        state.explicitData.movePointers( sid, 1 );
        state.layout.switchBB( getBB( state.control.getPC( tid ) ), tid );
    }

    void do_pthread_create( llvm::Value *child_fun_value, const llvm::Value *thread_id,
                            const llvm::Value *llvm_argument, int tid )
    {
        while ( llvm::isa< llvm::ConstantExpr >( child_fun_value ) ) {
            auto cexpr = llvm::cast< llvm::ConstantExpr >( child_fun_value );
            assert( cexpr->getOpcode() ==  llvm::Instruction::BitCast );
            child_fun_value = cexpr->getOperand( 0 );
        }
        const llvm::Function *child_fun = llvm::cast< llvm::Function >( child_fun_value );

        assert( functionmap.find( child_fun ) != functionmap.end() );
        int64_t new_tid = startThread( functionmap[ child_fun ] );
        int t_id_bw = getBitWidth( thread_id->getType() );

        Value tid_ptr = deref( thread_id, tid );
        assert( tid_ptr.type == Value::Type::Constant );
        Pointer tid_dest = Pointer( tid_ptr.constant.value );
        state.layout.setMultival( deref( tid_dest ), false );
        state.explicitData.implement_store( tid_dest, Value( new_tid, t_id_bw ) );

        if ( child_fun->arg_begin() != child_fun->arg_end() ) {
            Value argument = deref( llvm_argument, tid, false );
            Value argument_dest = deref( child_fun->arg_begin(), state.control.threadCount() - 1 );

            state.layout.setMultival( argument_dest, state.layout.isMultival( argument ) );
            if ( state.layout.isMultival( argument ) ) {
                state.data.implement_store( argument_dest, argument );
            } else {
                state.explicitData.implement_store( argument_dest, argument );
            }
        }
    }

    bool do_mutex_lock( const llvm::Value *mutex_id, int tid )
    {
        Value mutex_ptr = deref( mutex_id, tid );
        int m_id_bw = getBitWidth( mutex_id->getType() );
        assert( mutex_ptr.type == Value::Type::Constant );
        Pointer mutex_dest = Pointer( mutex_ptr.constant.value );

        if ( state.explicitData.get( mutex_dest ) )
            return false;
        state.layout.setMultival( deref( mutex_dest ), false );
        state.explicitData.implement_store( mutex_dest, Value( 1, m_id_bw ) );
        return true;
    }

    void do_mutex_unlock( const llvm::Value *mutex_id, int tid )
    {
        Value mutex_ptr = deref( mutex_id, tid );
        int m_id_bw = getBitWidth( mutex_id->getType() );
        assert( mutex_ptr.type == Value::Type::Constant );
        Pointer mutex_dest = Pointer( mutex_ptr.constant.value );

        state.layout.setMultival( deref( mutex_dest ), false );
        state.explicitData.implement_store( mutex_dest, Value( 0, m_id_bw ) );
    }

    template < typename Yield >
    void do_ite( const llvm::SelectInst *si, int tid, Yield yield )
    {
        Value cond = deref( si->getCondition(), tid );
        Value true_val = deref( si->getTrueValue(), tid );
        Value false_val = deref( si->getFalseValue(), tid );

        Value val = cond.constant.value ? true_val : false_val;
        llvm_sym::DataStore *store;
        bool multival = state.layout.isMultival( val );
        if ( multival )
            store = &state.data;
        else
            store = &state.explicitData;

        state.layout.setMultival( deref( si, tid, false ), state.layout.isMultival( val ) );
        if ( cond.type == Value::Type::Constant ) {
            store->implement_store( deref( si, tid, false ), val );
        } else {
            die( ErrorCause::SYMBOLIC_SELECT );
        }

        yield( false, false, true );
    }

    template < typename Yield >
    void do_call( const llvm::CallInst *ci, int tid, Yield yield )
    {
        const llvm::Value *child_fun_value = ci->getCalledFunction() ?
              ci->getCalledFunction()
            : ci->getCalledValue();

        while ( llvm::isa< llvm::ConstantExpr >( child_fun_value ) ) {
            auto cexpr = llvm::cast< llvm::ConstantExpr >( child_fun_value );
            assert( cexpr->getOpcode() ==  llvm::Instruction::BitCast );
            child_fun_value = cexpr->getOperand( 0 );
        }

        const llvm::Function *called_fun = llvm::cast< llvm::Function >( child_fun_value );

        state.control.advance( tid );

        if ( called_fun->empty() ) {
            std::string fun_name = Demangler::demangle( std::string( called_fun->getName() ) );
            if ( ignored_functions.count( fun_name ) ) {
                yield( false, false, true );
                return;
            } else if ( isFunctionInput( fun_name ) ) {
                Value to = deref( ci, tid, false );
                state.layout.setMultival( to, true );
                state.data.implement_input( to, getBitWidth( ci->getType() ) );
                yield( false, false, true );
            } else if ( isFunctionAssume( fun_name )) {
                llvm::Value *llvm_cond = ci->getArgOperand( 0 );
                Value cond = deref( llvm_cond, tid, false );
                llvm_sym::DataStore *store;
                if ( state.layout.isMultival( cond ) )
                    store = &state.data;
                else
                    store = &state.explicitData;
                store->prune( cond, Value( 0, getBitWidth( llvm_cond->getType() ) ), ICmp_Op::NE );
                yield( false, store->empty(), true );
            } else if ( fun_name == "pthread_create" ) {
                do_pthread_create( ci->getArgOperand( 2 ), ci->getArgOperand( 0 ), ci->getArgOperand( 3 ), tid );
                yield( true, false, true );
            } else if ( fun_name == "pthread_join" ) {
                const llvm::Value *tid_llvm = ci->getArgOperand( 0 );
                Value tid_val = deref( tid_llvm, tid );
                assert( tid_val.type == Value::Type::Constant );

                if ( state.control.hasTid( tid_val.constant.value ) )
                    state.control.advance( tid, -1 );
                yield( !amILonelyThread(), false, true );
            } else if ( fun_name == "exit" ) {
                while ( state.control.threadCount() )
                    state.control.leave( 0 );
                assert( state.control.threadCount() == 0 );
                yield( true, false, true );
            } else if ( fun_name == "pthread_exit" ) {
                while ( !state.control.leave( tid ) )
                    ;
                yield( true, false, true );
            } else if ( fun_name == "assert" || fun_name == "__VERIFIER_assert" ) {
                llvm::Value *llvm_cond = ci->getArgOperand( 0 );
                Value cond = deref( llvm_cond, tid, false );

                llvm_sym::DataStore *store;
                bool multival_arg = state.layout.isMultival( cond );
                if ( multival_arg )
                    store = &state.data;
                else
                    store = &state.explicitData;

                store->prune(
                        cond,
                        Value( 0, getBitWidth( llvm_cond->getType() ) ),
                        ICmp_Op::NE );
                yield( false, store->empty() );

                store->prune(
                        cond,
                        Value( 0, getBitWidth( llvm_cond->getType() ) ),
                        ICmp_Op::EQ );
                state.properties.setError( true );
                yield( true, store->empty(), true );

            } else if ( fun_name == "pthread_mutex_lock" ) {
                if ( !do_mutex_lock( ci->getArgOperand( 0 ), tid ) )
                    state.control.advance( tid, -1 );
                yield( !amILonelyThread(), false, true );
            } else if ( fun_name == "pthread_mutex_unlock" ) {
                do_mutex_unlock( ci->getArgOperand( 0 ), tid );
                yield( !amILonelyThread(), false, true );
            } else {
                std::cerr << "I don't know what to do with function "
                          << fun_name << std::endl;
                abort();
            }

        } else {
            assert( functionmap.find( ci->getCalledFunction() ) != functionmap.end() );
            std::vector< Value > params;
            const llvm::Function *fun_called = ci->getCalledFunction();
            
            for ( unsigned arg_no = 0; arg_no < ci->getNumArgOperands(); ++arg_no ) {
                params.push_back( deref( ci->getArgOperand( arg_no ), tid ) );
            }
            enterFunction( functionmap[ fun_called ], tid );

            auto arg = fun_called->arg_begin();
            for ( unsigned arg_no = 0; arg_no < params.size(); ++arg_no, ++arg) {
                Value arg_value = deref( arg, tid, false );
                state.layout.setMultival( arg_value, state.layout.isMultival( params[ arg_no ] ) );
                if ( state.layout.isMultival( params[ arg_no ] ) ) {
                    state.data.implement_store( arg_value, params[ arg_no ] );
                } else {
                    state.explicitData.implement_store( arg_value, params[ arg_no ] );
                }
            }
            yield( false, false, true );
        }
    }

    template < typename Yield >
    void do_alloca( const llvm::AllocaInst *alloca_inst, int tid, Yield yield )
    {
        Value array_size = deref( alloca_inst->getArraySize(), tid );
        if ( array_size.type != Value::Type::Constant )
            die( ErrorCause::VARIABLE_LEN_ARRAY );

        int sid = state.layout.newSegment( tid );
        std::vector< int > bws = getPrimitiveTypeWidths( alloca_inst->getAllocatedType() );
        Pointer ptr = Pointer( sid, 0 );

        state.data.addSegment( sid, bws );
        state.explicitData.addSegment( sid, bws );
        state.layout.addSegment( sid, bws );

        state.explicitData.movePointers( sid, 1 );

        state.layout.setMultival( deref( alloca_inst, tid, false ), false );
        state.explicitData.implement_pointer_store( deref( alloca_inst, tid, false ), ptr );
        yield( false, false, true );
    }

    void do_GEP( const llvm::User *inst, int tid )
    {
        llvm::Type *ty = inst->getOperand( 0 )->getType();

        assert( ty->isPointerTy() );
        Value idx = deref( inst->getOperand( 1 ), tid );

        if ( idx.type != Value::Type::Constant ) {
            die( ErrorCause::SYMBOLIC_POINTER );
        }

        int offset = idx.constant.value * getElementsWidth( ty->getPointerElementType() );
        ty = ty->getPointerElementType();
        for ( unsigned idx = 2; idx < inst->getNumOperands(); ++idx ) {
            Value idx_val = deref( inst->getOperand( idx ), tid );
            assert( idx_val.type == Value::Type::Constant );

            assert( ty->isStructTy() || ty->isArrayTy() );
            if ( ty->isStructTy() ) {
                for ( int i = 0; i < idx_val.constant.value; ++i ) {
                    offset += getElementsWidth( ty->getContainedType( i ) );
                }
            } else {
                offset = idx_val.constant.value * getElementsWidth( ty->getContainedType( 0 ) );
            }

            if ( ty->isStructTy() )
                ty = ty->getContainedType( idx_val.constant.value );
            else if ( ty->isArrayTy() )
                ty = ty->getContainedType( 0 );
        }

        Pointer ptr = Pointer( deref( inst->getOperand( 0 ), tid ).constant.value );
        ptr.content.offset += offset;

        state.layout.setMultival( deref( inst, tid, false ), false );
        state.explicitData.implement_pointer_store( deref( inst, tid, false ), ptr );
    }

    template < typename Yield >
    void do_load( const llvm::LoadInst *inst, int tid, Yield yield )
    {
        Value result = deref( inst, tid, false );
        const llvm::Value *ptr_operand = inst->getPointerOperand();
        Value val = deref( ptr_operand, tid );
        assert( val.type == Value::Type::Constant );
        uint64_t ptr_content = val.constant.value;
        Pointer from_ptr = static_cast< Pointer >( ptr_content );
        Value from;
        from.type = Value::Type::Variable;
        from.variable = from_ptr.content;
        from.pointer = inst->getType()->isPointerTy();

        state.layout.setMultival( result, state.layout.isMultival( from ) );
        if ( state.layout.isMultival( from ) )
            state.data.implement_store( result, from );
        else
            state.explicitData.implement_store( result, from );
        yield( !amILonelyThread(), false, true );
    }

    template < typename Yield >
    void do_store( const llvm::StoreInst *store_inst, int tid, Yield yield )
    {
        Value value = deref( store_inst->getValueOperand(), tid );
        const llvm::Value *ptr_operand = store_inst->getPointerOperand();
        Value val = deref( ptr_operand, tid, false );
        Pointer to_ptr = static_cast< Pointer >( state.explicitData.get( val ) );
        Value to;
        to.type = Value::Type::Variable;
        to.variable = to_ptr.content;
        to.pointer = store_inst->getValueOperand()->getType()->isPointerTy();

        state.layout.setMultival( to, state.layout.isMultival( value ) );
        if ( state.layout.isMultival( value ) )
            state.data.implement_store( to, value );
        else
            state.explicitData.implement_store( to, value );
        yield( !amILonelyThread(), false, true );
    }

    template < typename Yield >
    void do_phi( const llvm::PHINode *phi, int tid, Yield yield )
    {
        auto &previous_bb = state.control.previous_bb[ tid ];
        llvm::BasicBlock *prev_bb = getBB( previous_bb ).bb;
        assert( prev_bb );
        const llvm::Value* incoming_value =
            phi->getIncomingValueForBlock( prev_bb );

        assert( incoming_value );

        Value incoming = deref( incoming_value, tid );
        Value result = deref( phi, tid, false );

        state.layout.setMultival( result, state.layout.isMultival( incoming ) );
        if ( state.layout.isMultival( incoming ) )
            state.data.implement_store( result, incoming );
        else
            state.explicitData.implement_store( result, incoming );

        yield( false, false, true );
    }

    template < typename Yield >
    void do_icmp( const llvm::ICmpInst *cmp_inst, int tid, Yield yield )
    {
        auto a = deref( cmp_inst->getOperand( 0 ), tid );
        auto b = deref( cmp_inst->getOperand( 1 ), tid );
        bool a_is_multival = state.layout.isMultival( a );
        bool b_is_multival = state.layout.isMultival( b );
        auto result = deref( cmp_inst, tid, false );

        int result_bw = getBitWidth( cmp_inst->getType() );
        assert( result_bw == 1 );

        llvm_sym::DataStore *store;
        if ( a_is_multival || b_is_multival )
            store = &state.data;
        else
            store = &state.explicitData;
        store->prune( a, b, ICmp_Op( cmp_inst->getPredicate() ) );

        state.layout.setMultival( result, false );
        state.explicitData.implement_store( result, Value( 1, result_bw ) );
        assert( state.explicitData.get( result ) == 1 );
        yield( false, store->empty() );

        store->prune( a, b, icmp_negate( ICmp_Op( cmp_inst->getPredicate() ) ) );

        state.layout.setMultival( result, false );
        state.explicitData.implement_store( result, Value( 0, result_bw ) );
        assert( state.explicitData.get( result ) == 0 );
        yield( false, store->empty(), true );
    }

    template < typename Yield >
    void do_break( const llvm::BranchInst *bri, int tid, Yield yield )
    {

        if ( bri->isUnconditional() ) {
            assert( bri->getNumSuccessors() == 1 );
            jumpTo( bri->getSuccessor( 0 ), tid );
            bool is_backward_br = bri->getSuccessor( 0 ) <= bri->getParent();
            yield( is_backward_br, false, true );
        } else {
            assert( bri->getNumSuccessors() == 2 );
            for ( int branch = 0; branch < 2; ++branch ) {
                auto cond = deref( bri->getCondition(), tid );

                Value val = Value( branch == 0 ? 1lu : 0lu, 1 );
                jumpTo( bri->getSuccessor( branch ), tid );
                bool is_backward_br = bri->getSuccessor( branch ) <= bri->getParent();

                llvm_sym::DataStore *store;
                if ( state.layout.isMultival( cond ) )
                    store = &state.data;
                else
                    store = &state.explicitData;
                store->prune( cond, val, ICmp_Op::EQ );

                yield( is_backward_br, store->empty(), branch == 1 );
            }
        }
    }

    template < typename Yield >
    void do_switch( const llvm::SwitchInst* swi, int tid, Yield yield )
    {
        llvm::Value *condition_llvm = swi->getCondition();
        Value condition = deref( condition_llvm, tid );
        bool multivalue = state.layout.isMultival( condition );
        llvm_sym::DataStore *store;
        if ( multivalue )
            store = &state.data;
        else
            store = &state.explicitData;

        auto iE = swi->case_end();
        for ( auto it = swi->case_begin(); it != iE; ++it ) {
            store->prune( condition, deref( it.getCaseValue(), tid ), ICmp_Op::EQ );
            jumpTo( it.getCaseSuccessor(), tid );
            bool is_backward_br = it.getCaseSuccessor() <= swi->getParent();
            yield( is_backward_br, store->empty() );
        }

        // default case
        for ( auto it = swi->case_begin(); it != iE; ++it ) {
            store->prune( condition, deref( it.getCaseValue(), tid ), ICmp_Op::NE );
        }
        bool is_backward_br = swi->getDefaultDest() <= swi->getParent();
        jumpTo( swi->getDefaultDest(), tid );
        yield( is_backward_br, store->empty(), true );
    }

    template < typename Yield >
    void do_cast( const llvm::CastInst *inst, int tid, Yield yield )
    {
        assert( inst->getNumOperands() == 1 );
        Value a = deref( inst->getOperand( 0 ), tid );

        int to_bw = getBitWidth( inst->getDestTy() );

        Value result = deref( inst, tid, false );
        state.layout.setMultival( result, state.layout.isMultival( a ) );

        llvm_sym::DataStore *store;
        if ( state.layout.isMultival( a ) )
            store = &state.data;
        else
            store = &state.explicitData;

        switch ( inst->getOpcode() ) {
            case llvm::Instruction::ZExt:
            case llvm::Instruction::FPExt:
                store->implement_ZExt( result, a, to_bw );
                break;
            case llvm::Instruction::SExt:
                store->implement_SExt( result, a, to_bw );
                break;
            case llvm::Instruction::FPTrunc:
            case llvm::Instruction::Trunc:
                store->implement_Trunc( result, a, to_bw );
                break;
            default:
                assert( false );
        }

        yield( false, false, true );
    }

    template < typename Yield >
    void do_return( const llvm::ReturnInst *reti, int tid, Yield yield )
    {
        const llvm::Value *returned = reti->getReturnValue();
        if ( state.control.last( tid ) || !returned || llvm::isa< llvm::UndefValue >( returned ) ) {
            leave( tid );
        } else {
            assert( returned && !llvm::isa< llvm::UndefValue >( returned ) );
            PC prev_pc = state.control.getPrevPC( tid );
            // we have advanced after Call, we need to get back
            // to obtain the Call instruction
            --prev_pc.instruction;
            const BB &prev_bb = getBB( prev_pc );

            Value calee_return_val = deref( returned, tid );
            state.layout.switchBB( prev_bb, tid );
            Value caller_return_val = deref( fetchPC( tid, prev_pc ), tid, false, true );

            state.layout.setMultival( caller_return_val, state.layout.isMultival( calee_return_val ) );
            if ( state.layout.isMultival( calee_return_val ) )
                state.data.implement_store( caller_return_val, calee_return_val );
            else {
                state.explicitData.implement_store( caller_return_val, calee_return_val );
            }

            leave( tid );
        }
        yield( true, false, true );
    }

    void do_binary_arithmetic_op( unsigned opcode, llvm_sym::DataStore &store, Value res, Value a, Value b, int tid )
    {
        bool multival = state.layout.isMultival( a ) || state.layout.isMultival( b );
        state.layout.setMultival( res, multival );
        switch( opcode ) {
            case llvm::Instruction::Add: {
                store.implement_add( res, a, b );
                break;
            }
            case llvm::Instruction::FSub:
            case llvm::Instruction::Sub:
                store.implement_sub( res, a, b );
                break;
            case llvm::Instruction::FMul:
            case llvm::Instruction::Mul:
                store.implement_mult( res, a, b );
                break;
            case llvm::Instruction::FDiv:
            case llvm::Instruction::SDiv:
            case llvm::Instruction::UDiv:
                store.implement_div( res, a, b );
                break;
            case llvm::Instruction::FRem:
            case llvm::Instruction::URem:
                store.implement_urem( res, a, b );
            case llvm::Instruction::SRem:
                store.implement_srem( res, a, b );
                break;
            case llvm::Instruction::And:
                store.implement_and( res, a, b );
                break;
            case llvm::Instruction::Or:
                store.implement_or( res, a, b );
                break;
            case llvm::Instruction::Xor:
                store.implement_xor( res, a, b );
                break;
            case llvm::Instruction::Shl:
                store.implement_left_shift( res, a, b );
                break;
            case llvm::Instruction::LShr:
                store.implement_right_shift( res, a, b );
                break;
            default:
                abort();
        }
    }

    BB actualBB( int tid ) const
    {
        const PC &pc = state.control.getPC( tid );
        return functions[ pc.function ].body[ pc.basicblock ];
    }

    public:

  DataStore* getData() {
    return &(state.data);
  }

  State * getState() {
    return &state;
  }

    bool is_global_zero( std::string name )
    {
        assert( bc );
        for ( const auto &g : bc->module.get()->getGlobalList() ) {
            if ( g.getName() == name ) {
                Pointer global_ptr = state.explicitData.get( deref( &g, -1 ) );
                Value global_content = deref( global_ptr );
                if ( global_content.type != Value::Type::Constant ) {
                    std::cerr << "unsupported: atomic proposition"
                                 " with symbolic value" << std::endl;
                    abort();
                }
                return global_content.constant.value == 0;
            }
        }
        std::cerr << "error: atomic proposition with nonexistent"
                     " global variable" << std::endl;
        abort();
    }

    int getExplicitSize() const
    {
        return state.control.getSize() + state.layout.getSize()
            + state.explicitData.getSize() + state.properties.getSize();
    }

    Evaluator( std::shared_ptr< BitCode > b ) : main( nullptr ), state( b.get()->module.get() ), bc( b )
    {
        bc = b;
        
        llvm::Module::iterator fun_iter;
        main = nullptr;
        for ( fun_iter = bc->module->begin(); fun_iter != bc->module->end(); ++fun_iter ) {
            functionmap[ fun_iter ] = functions.size();
            functions.emplace_back( fun_iter );
            if ( fun_iter->getName() == "main" )
                main = fun_iter;
        }

        if ( !main ) {
            std::cerr << "Missing function main." << std::endl;
            abort();
        }

        for ( short unsigned i = 0; i < functions.size(); ++i ) {
            int bb_id = 0;
            for ( auto block : functions[ i ].body ) {
                PC pc;

                pc.function = i;
                pc.basicblock = bb_id++;
                pc.instruction = 0;
                
                blockmap[ block.bb ] = pc;
            }
        }
        initial();
    }

    void initial()
    {
        assert( bc->module );
        state.control.clear();
        state.data.clear();
        state.explicitData.clear();
        state.properties.setError( false );

        int globals_count = bc->module.get()->getGlobalList().size();
        std::vector< int > global_variables;

        std::vector< int > globals_bitwidths;
        std::vector< int > globals_expl_bitwidths = std::vector< int >( globals_count, 64 );

        global_variables = getGlobalsWidths();
        getGlobalsBitWidths( globals_bitwidths );

        state.explicitData.addSegment( 0, globals_expl_bitwidths );
        state.data.addSegment( 0, std::vector< int >() );
        state.layout.addSegment( 0, globals_expl_bitwidths );

        state.data.addSegment( 1, globals_bitwidths );
        state.explicitData.addSegment( 1, globals_bitwidths );
        state.layout.addSegment( 1, globals_bitwidths );

        // store pointers to globals into explicit memory
        int offset = 0;
        auto g_var_it = bc->module->getGlobalList().begin();
        auto g_width_it = global_variables.begin();
        int count = 0;
        for ( ; g_var_it != bc->module->getGlobalList().end(); ++g_var_it, ++g_width_it ) {
            assert( g_var_it != bc->module->getGlobalList().end() );
            Value global_ptr;
            global_ptr.type = Value::Type::Variable;
            global_ptr.variable.segmentId = 0;
            global_ptr.variable.offset = count++;
            global_ptr.pointer = true;

            Pointer ptr_to_global( 1, offset );
            offset += *g_width_it;

            if ( g_var_it->hasInitializer() ) {
                if ( g_var_it->getInitializer()->getType()->isAggregateType() ) {
                    Pointer ptr_to_global_aux = ptr_to_global;
                    for ( int i = 0; i < *g_width_it; ++i ) {
                        state.layout.setMultival( deref( ptr_to_global_aux ), false );
                        state.explicitData.implement_store( deref( ptr_to_global_aux ), Value( 0, 64 ) );
                        ++ptr_to_global_aux.content.offset;
                    }

                    llvm::isa< llvm::ConstantAggregateZero >( g_var_it->getInitializer() );
                } else if ( g_var_it->getInitializer()->getType()->isIntegerTy() ) {
                    Value initializer = deref( g_var_it->getInitializer(), -1 );
                    assert( initializer.type == Value::Type::Constant );

                    state.layout.setMultival( deref( ptr_to_global ), false );
                    state.explicitData.implement_store( deref( ptr_to_global ), initializer );
                } else {
                    if ( Config::getOption( "verbose" ) ) {
                        std::cerr << "initializer for "; g_var_it->dump();
                        std::cerr << " not recognized. Skipping." << std::endl;
                    }
                }
            }

            state.layout.setMultival( global_ptr, false );
            Value ptr_content = Value( static_cast< uint64_t >( ptr_to_global ), 64 );
            state.explicitData.implement_store( global_ptr, ptr_content );
        }

        assert( functionmap.count( main ) > 0 );

        startThread( functionmap[ main ] );
    }

    int startThread( unsigned short fun_id )
    {
        int tid = state.control.startThread( fun_id );
        int last_thread = state.control.threadCount() - 1;
        BB bb = getBB( state.control.getPC( last_thread ) );
        state.layout.startThread();
        unsigned sid = state.layout.getLastStackSegmentRange( last_thread ).first;

        std::vector< int > bitwidths = getBitWidthList( fun_id );

        state.data.addSegment( sid, bitwidths );
        state.explicitData.addSegment( sid, bitwidths );
        state.layout.addSegment( sid, bitwidths );

        state.explicitData.movePointers( sid, 1 );

        state.layout.switchBB( bb, last_thread );

        return tid;
    }

    size_t getSize() const
    {
        return state.getSize();
    }

    void write( char *mem ) const
    {
        char *orig_mem = mem;
        state.properties.writeData( mem );
        state.control.writeData( mem );
        state.layout.writeData( mem );
        state.explicitData.writeData( mem );
        state.data.writeData( mem );
        assert( state.getSize() == static_cast< unsigned >( mem - orig_mem ) );
    }

    void read( const char *mem )
    {
        const char *orig_mem = mem;
        state.properties.readData( mem );
        state.control.readData( mem );
        state.layout.readData( mem );
        state.explicitData.readData( mem );
        state.data.readData( mem );
        assert( state.getSize() == static_cast< unsigned >( mem - orig_mem ) );

        for ( unsigned i = 0; i < state.control.threadCount(); ++i ) {
            state.layout.switchBB( actualBB( i ), i );
        }
    }

    void advance( const std::function<void ()> yield )
    {
        std::string readS, writeS;
        int threads = state.control.threadCount();
        for ( int tid = 0; tid < threads; ++tid) {
            std::stack< State > to_do;
            to_do.push( std::move( state ) );

            // do a graph traversal from current state, stop on observable
            // states (leaves in the traversal) and yield them
            while( !to_do.empty() ) {
                State snapshot = to_do.top();
                state = std::move( to_do.top() );
                if ( Config::getOption( "vverbose" ) )
                    std::cerr << "---------\nin state:\n" << toString() << std::endl;
                to_do.pop();

                bool last_check = false;
                auto restoringYield = [&to_do, &last_check, this, &snapshot, &yield]( bool is_observable,
                                                                         bool is_empty,
                                                                         bool last = false )
                {
                    assert( !last_check );
                    if ( !is_empty ) {
                        if ( is_observable || isError() ) {
                            yield();
                        } else {
                            to_do.push( std::move( state ) );
                        }
                    }
                    if ( !last )
                        state = snapshot;
                    else {
                        state = std::move( snapshot );
                        last_check = true;
                    }
                };

                advance( restoringYield, tid );
            }
        }
    }

    template < typename Yield >
    void advance( Yield yield, int tid )
    {
        if ( state.properties.isError() )
            return;
        llvm::Instruction *inst = fetch( tid );

        auto effect = [tid, inst, this]() {
            if ( !llvm::isa< llvm::TerminatorInst >( inst ) && !llvm::isa< llvm::CallInst >( inst ) ) {
                state.control.advance( tid );
            }
        };

        /* Execute fetched instruction. Each yield() call will clear all prior
         * changes done on this state - therefore, we need to do 'effect' after
         * each yield() call
         */
        if ( Config::getOption( "vverbose" ) ) {
            std::cerr << "executing instruction " << std::string( functions[state.control.getPC(tid).function].llvm_fun->getName() )
                << "." << state.control.getPC( tid ).basicblock
                << "." << state.control.getPC( tid ).instruction << std::endl;
            std::cerr << "\t"; inst->dump(); std::cerr << std::endl;
        }
        if ( !llvm::isa< llvm::PHINode >( inst ) )
            state.control.previous_bb[ tid ] = PC();

        InstDispatch::execute( inst, tid, makeYieldEffect( yield, effect ) );
    
    }

    bool isError() const
    {
        return state.properties.isError();
    }

    std::string toString() const
    {
        std::stringstream ss;
        if ( state.control.context.empty() )
            return "exit";
        
        if ( isError() )
            ss << "Error state." << std::endl;
        ss << "Control:\n" << state.control;
        ss << "Data:\n" << state.layout;
        ss << state.explicitData;
        ss << state.data;

        return ss.str();
    }

    friend class Dispatcher< Evaluator< DataStore > >;
};

}

