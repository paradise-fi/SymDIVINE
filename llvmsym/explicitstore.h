#pragma once

#include <llvmsym/datastore.h>
#include <llvmsym/blobutils.h>
#include <vector>

namespace llvm_sym {

template < typename ElementRepr >
class ValueStore : public DataStore {
    typedef typename ElementRepr::element Element;
    typedef std::vector< Element > Segment;
    typedef std::vector< Segment > Data;
    Data _data;

    struct VariableInfo {
        char bw;
        bool is_pointer;
    };

    std::vector< std::vector< VariableInfo > > _info;
    bool _empty;

    Element &at( Value a ) {
        assert( a.type == Value::Type::Variable );
        assert( a.variable.segmentId < _data.size() );
        assert( a.variable.offset < _data[ a.variable.segmentId ].size() );
        return _data[ a.variable.segmentId ][ a.variable.offset ];
    }
    
    const Element &at( Value a ) const {
        assert( a.type == Value::Type::Variable );
        assert( a.variable.segmentId < _data.size() );
        assert( a.variable.offset < _data[ a.variable.segmentId ].size() );
        return _data[ a.variable.segmentId ][ a.variable.offset ];
    }

    void setPointerFlag( Value a, bool flag ) {
        assert( a.type == Value::Type::Variable );
        assert( a.variable.segmentId < _data.size() );
        assert( a.variable.offset < _data[ a.variable.segmentId ].size() );
        _info[ a.variable.segmentId ][ a.variable.offset ].is_pointer = flag;
    }

    int getBw( Value a ) const {
        if ( a.type == Value::Type::Constant )
            return a.constant.bw;
        assert( a.type == Value::Type::Variable );
        assert( a.variable.segmentId < _data.size() );
        assert( a.variable.offset < _data[ a.variable.segmentId ].size() );
        return _info[ a.variable.segmentId ][ a.variable.offset ].bw;
    }

    static uint64_t lower_to_nbits( uint64_t what, int bw ) {
        if ( bw == 64 )
            return what;
        uint64_t mask = (1llu << bw) - 1;
        return what & mask;
    }

    public:
    
    struct Pointer {
        typename DataStore::VariableId content;

        Pointer( unsigned seg, unsigned off ) : content( { seg, off } ) {}
        explicit Pointer( uint64_t i )
        {
            uint64_t lower_mask = ((1llu << 32) - 1);
            content.segmentId = i >> 32;
            content.offset = i & lower_mask;
        }

        operator uint64_t() const
        {
            return (static_cast< uint64_t >( content.segmentId ) << 32) | content.offset;
        }
    };


    const Element get( Pointer a ) const
    {
        Value p;
        p.type = Value::Type::Variable;
        p.variable = a.content;
        return get( p );
    }

    const Element get( Value a ) const
    {
        if ( a.type == Value::Type::Constant )
            return Element( a.constant.value );
        else {
            assert( a.variable.segmentId < _data.size() );
            assert( a.variable.offset < _data[ a.variable.segmentId ].size() );
            return _data[ a.variable.segmentId ][ a.variable.offset ];
        }
    }

    virtual size_t getSize() const
    {
        return representation_size( _data, _info, _empty );
    }

    ValueStore() : _empty( false ) 
    {}

    virtual void writeData( char * &mem ) const
    {
        static_assert(!std::is_pod< Data >::value, "Data must be POD" );
        blobWrite( mem, _data, _info, _empty );
    }

    virtual void readData( const char * &mem )
    {
        _data.clear();
        blobRead( mem, _data, _info, _empty );
    }

    virtual void addSegment( unsigned id, const std::vector< int > &bit_widths )
    {
        assert( id <= _data.size() );
        _data.emplace( _data.begin() + id, bit_widths.size(), Element() );
        std::vector< VariableInfo >& infos = *_info.insert( _info.begin() + id, std::vector< VariableInfo >() );
        for ( int bw : bit_widths ) {
            infos.push_back( VariableInfo( { static_cast< char >( bw ), false } ) );
        }
    }

    virtual void eraseSegment( int id )
    {
        _data.erase( _data.begin() + id );
        _info.erase( _info.begin() + id );
    }

    void movePointers( unsigned from, int move_count )
    {
        for ( unsigned seg = 0; seg < _data.size(); ++seg ) {
            for ( unsigned offset = 0; offset < _data[seg].size(); ++offset ) {
                if ( !_info[ seg ][ offset ].is_pointer )
                    continue;

                Pointer ptr = Pointer( _data[ seg ][ offset ] );

                if ( ptr.content.segmentId > from ) {
                    ptr.content.segmentId += move_count;
                    _data[ seg ][ offset ] = static_cast< uint64_t >( ptr );
                }

            }
        }
    }

    void clear()
    {
        _data.clear();
        _info.clear();
    }

    virtual void implement_add( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) + get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_sub( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) - get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_div( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) / get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }
    
    virtual void implement_srem( Value result, Value a, Value b )
    {
        switch ( getBw( a ) ) {
            case 64:
                at( result ) = lower_to_nbits(
                          static_cast< int64_t >( get( a ) )
                        % static_cast< int64_t >( get( b ) ),
                                getBw( result ) );
                break;
            case 32:
                at( result ) = lower_to_nbits(
                          static_cast< int32_t >( get( a ) )
                        % static_cast< int32_t >( get( b ) ),
                                getBw( result ) );
                break;
            case 16:
                at( result ) = lower_to_nbits(
                          static_cast< int16_t >( get( a ) )
                        % static_cast< int16_t >( get( b ) ),
                                getBw( result ) );
                break;
            case 8:
                at( result ) = lower_to_nbits(
                          static_cast< int8_t >( get( a ) )
                        % static_cast< int8_t >( get( b ) ),
                                getBw( result ) );
                break;
            default:
                assert( false );
                abort();
        }
        setPointerFlag( result, a.pointer || b.pointer );
    }
    
    virtual void implement_urem( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) % get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_mult( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) * get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_and( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) & get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_or( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) | get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_xor( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) ^ get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_left_shift( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) << get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_right_shift( Value result, Value a, Value b )
    {
        at( result ) = lower_to_nbits( get( a ) >> get( b ), getBw( result ) );
        setPointerFlag( result, a.pointer || b.pointer );
    }

    virtual void implement_store( Value result, Value what )
    {
        at( result ) = lower_to_nbits( get( what ), getBw( result ) );
        setPointerFlag( result, what.pointer );
    }

    void implement_store( Pointer result_ptr, Value what )
    {
        Value result;
        result.type = Value::Type::Variable;
        result.variable = result_ptr.content;
        at( result ) = lower_to_nbits( get( what ), getBw( result ) );
        setPointerFlag( result, what.pointer );
    }

    void implement_pointer_store( Value result, Pointer what )
    {
        implement_store( result, Value( static_cast< uint64_t >( what ), 64, true ) );
    }

    virtual void implement_ZExt( Value result_id, Value a_id, int bw )
    {
        implement_store( result_id, a_id );
    }
    virtual void implement_SExt( Value result_id, Value a_id, int bw )
    {
        implement_store( result_id, a_id );
    }

    virtual void implement_Trunc( Value result_id, Value a_id, int bw )
    {
        implement_store( result_id, a_id );
    }
    
    virtual void implement_input( Value input_variable, unsigned bw )
    {
        assert( false );
    }
    
    virtual bool equal( char *mem_a, char *mem_b ) const
    {
        return memcmp( mem_a, mem_b, getSize() ) == 0;
    }
    
    virtual void prune( Value a, Value b, ICmp_Op op )
    {
        assert( getBw( a ) == getBw( b ) );
        bool holds;
        switch ( op ) {
            case ICmp_Op::EQ:
                holds = get( a ) == get( b );
                break;
            case ICmp_Op::NE:
                holds = get( a ) != get( b );
                break;
            case ICmp_Op::UGT:
                holds = get( a ) > get( b );
                break;
            case ICmp_Op::SGT:
                switch ( getBw( a ) ) {
                    case 64:
                        holds = static_cast< int64_t >( get( a ) )
                                > static_cast< int64_t >( get ( b ) );
                        break;
                    case 32:
                        holds = static_cast< int32_t >( get( a ) )
                                > static_cast< int32_t >( get ( b ) );
                        break;
                    case 16:
                        holds = static_cast< int16_t >( get( a ) )
                                > static_cast< int16_t >( get ( b ) );
                        break;
                    case 8:
                        holds = static_cast< int8_t >( get( a ) )
                                > static_cast< int8_t >( get ( b ) );
                        break;
                    default:
                        assert( false );
                        abort();
                }
                break;
            case ICmp_Op::SGE:
                switch ( getBw( a ) ) {
                    case 64:
                        holds = static_cast< int64_t >( get( a ) )
                                >= static_cast< int64_t >( get ( b ) );
                        break;
                    case 32:
                        holds = static_cast< int32_t >( get( a ) )
                                >= static_cast< int32_t >( get ( b ) );
                        break;
                    case 16:
                        holds = static_cast< int16_t >( get( a ) )
                                >= static_cast< int16_t >( get ( b ) );
                        break;
                    case 8:
                        holds = static_cast< int8_t >( get( a ) )
                                >= static_cast< int8_t >( get ( b ) );
                        break;
                    default:
                        assert( false );
                        abort();
                }
                break;
            case ICmp_Op::UGE:
                holds = get( a ) >= get( b );
                break;
            case ICmp_Op::SLT:
                switch ( getBw( a ) ) {
                    case 64:
                        holds = static_cast< int64_t >( get( a ) )
                                < static_cast< int64_t >( get ( b ) );
                        break;
                    case 32:
                        holds = static_cast< int32_t >( get( a ) )
                                < static_cast< int32_t >( get ( b ) );
                        break;
                    case 16:
                        holds = static_cast< int16_t >( get( a ) )
                                < static_cast< int16_t >( get ( b ) );
                        break;
                    case 8:
                        holds = static_cast< int8_t >( get( a ) )
                                < static_cast< int8_t >( get ( b ) );
                        break;
                    default:
                        assert( false );
                        abort();
                }
                break;
            case ICmp_Op::ULT:
                holds = get( a ) < get( b );
                break;
            case ICmp_Op::SLE:
                switch ( getBw( a ) ) {
                    case 64:
                        holds = static_cast< int64_t >( get( a ) )
                                <= static_cast< int64_t >( get ( b ) );
                        break;
                    case 32:
                        holds = static_cast< int32_t >( get( a ) )
                                <= static_cast< int32_t >( get ( b ) );
                        break;
                    case 16:
                        holds = static_cast< int16_t >( get( a ) )
                                <= static_cast< int16_t >( get ( b ) );
                        break;
                    case 8:
                        holds = static_cast< int8_t >( get( a ) )
                                <= static_cast< int8_t >( get ( b ) );
                        break;
                    default:
                        assert( false );
                        abort();
                }
                break;
            case ICmp_Op::ULE:
                holds = get( a ) <= get( b );
                break;
        };
        _empty = _empty || !holds;
    }

    virtual bool empty() const
    {
        return _empty;
    }

    template < typename Element >
    friend std::ostream & operator<<( std::ostream & o, const ValueStore< Element > &v );
};

template < typename Element >
std::ostream & operator<<( std::ostream & o, const ValueStore< Element > &v )
{
    o << "data:\n";

    for ( unsigned sid = 0; sid < v._data.size(); ++sid ) {
        o << "stack No. " << sid << ":\n";

        o << "[ ";
        for ( auto &i : v._data[sid] )
            o << std::hex << i << " ";
        o << "]\n";
    }
        
    return o;
}

struct Explicit {
    typedef uint64_t element;
};

typedef ValueStore< Explicit > ExplicitStore;

}
