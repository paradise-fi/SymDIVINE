#include <llvmsym/memorylayout.h>

namespace llvm_sym {

std::ostream & operator<<( std::ostream & o, const MemoryLayout &m )
{
    for ( unsigned i = 0; i < m.thread_segments.size(); ++i ) {
        o << "tid " << i << " segments:\n";
        for ( auto j : m.thread_segments[i] )
            o << j << std::endl;
    }
    return o;
}

void MemoryLayout::leave( unsigned tid )
{
    assert( tid < thread_segments.size() );
    assert( !thread_segments[tid].empty() );
    
    dropLastStack( tid );
}

int MemoryLayout::newSegment( unsigned tid )
{
    assert( tid < thread_segments.size() );
    
    int wanted_sid;
    if ( thread_segments[tid].empty() ) {
        if ( tid == 0 )
            wanted_sid = 2;
        else
            wanted_sid = thread_segments[tid-1].back() + 1;
    } else
        wanted_sid = thread_segments[tid].back() + 1;

    assert( wanted_sid > 1 );
    segments_to_tid.insert( segments_to_tid.begin() + wanted_sid, tid );
    
    thread_segments[ tid ].emplace_back( wanted_sid );
    for ( unsigned i = tid+1; i < thread_segments.size(); ++i ) {
        for ( auto &sid : thread_segments[ i ] )
            ++sid;
    }
    ++segments_in_stack[ tid ].back();

    return wanted_sid;
}

DataStore::Value MemoryLayout::deref( const llvm::Value *v, int tid, bool prev ) const
{
    bool is_ptr = v->getType()->isPointerTy();
    if ( llvm::isa< llvm::ConstantInt >( v ) ) {
        const llvm::ConstantInt *const_int = llvm::cast< llvm::ConstantInt >( v );
        Value ret = Value( const_int->getLimitedValue(), const_int->getBitWidth(), is_ptr );
        return ret;
    }

    if ( llvm::isa< llvm::ConstantPointerNull >( v ) ) {
        Value ret = Value( 0, 64, true );
        return ret;
    }

    if ( llvm::isa< llvm::UndefValue >( v ) ) {
        const llvm::UndefValue *undef_val = llvm::cast< llvm::UndefValue >( v );
        return Value( 0, undef_val->getType()->getIntegerBitWidth(), is_ptr );
    }

    int varId = -1, segId = -1;

    std::map< const llvm::Value*, ValueInfo >::const_iterator f;
    if ( tid != -1
            && ( f = current_frames[ tid ]->valuemap.find( v ) )
                != current_frames[ tid ]->valuemap.end() )
    {
        varId = f->second.id;
        segId = getLastStackSegmentRange( tid, prev ).first;
        assert( segId > 1 );
    } else {
        f = global_valuemap->find( v );
        assert( f != global_valuemap->end() );
        varId = f->second.id;
        segId = 0;
    }

    assert( varId != -1 );
    assert( segId != -1 );

    Value ret;
    VariableId var;
    ret.type = Value::Type::Variable;
    var.segmentId = segId;
    var.offset = varId;
    ret.variable = var;
    ret.pointer = is_ptr;
    return ret;
}

}
