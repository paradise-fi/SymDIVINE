#pragma once

#include <llvmsym/llvmwrap/Instructions.h>
#include <llvmsym/llvmwrap/Function.h>
#include <llvmsym/llvmwrap/Constants.h>

#include <iostream>

#include <llvmsym/blobutils.h>
#include <llvmsym/llvmdata.h>
#include <llvmsym/datastore.h>

#include <llvmsym/stanalysis/inputvariables.h>

namespace llvm_sym {

class MemoryLayout {
    typedef typename DataStore::Value Value;
    typedef typename DataStore::Constant Constant;
    typedef typename DataStore::VariableId VariableId;

    struct ValueInfo {
        int id;
    };
    struct Frame {
        std::map< const llvm::Value*, ValueInfo > valuemap;
        llvm::BasicBlock *bb;

        Frame( BB b )
        {
            assert( b.bb );
            assert( b.bb->size() > 0 );

            const llvm::Function *parent_function = b.bb->getParent();
            assert( parent_function );

            auto values = collectUsedValues( parent_function );

            int next_index = 0;

            for ( const llvm::Value *v : *values ) {
                int elements_width = getElementsWidth( v->getType() );
                assert( elements_width == 1 ); // for now, structures cannot be stored in registers. TBD
                valuemap[ v ] = { next_index };
                next_index += elements_width;
            }
        }

    };
    std::vector< std::shared_ptr< const Frame > > current_frames;
    
    typedef std::map< const llvm::Value*, ValueInfo > ValueMap;
    std::shared_ptr< ValueMap > global_valuemap;
    std::vector< std::vector< short unsigned > > thread_segments;
    std::vector< std::vector< short unsigned > > segments_in_stack;
    std::vector< short unsigned > segments_to_tid;

    std::vector< std::vector< char > > variablesFlags;
    enum {
        F_MULTIVAL = 0x1,
        F_DEFAULT = F_MULTIVAL,
    };
    
    public:

    MemoryLayout( const llvm::Module* m ) : global_valuemap( std::make_shared< ValueMap >() )
    {
        clear();
        global_valuemap->clear();
        int globals = 0;
        for ( auto g = m->getGlobalList().begin(); g != m->getGlobalList().end(); ++g ) {
            int value_index = globals++;
            (*global_valuemap)[ g ] = { value_index };
        }
    }

    void clear()
    {
        current_frames.clear();
        segments_in_stack.clear();
        thread_segments.clear();
        segments_to_tid.clear();
        segments_to_tid.push_back( -1 ); // pointers to globals
        segments_to_tid.push_back( -1 ); // actual globals
    }

    void readData( const char * &mem )
    {
        clear();
        blobRead( mem, thread_segments, segments_in_stack, variablesFlags );
        
        int segments_count = 2;
        for ( const auto &ts : thread_segments )
            segments_count += ts.size();
        segments_to_tid.resize( segments_count );
        for ( size_t tid = 0; tid < thread_segments.size(); ++tid ) {
            for ( auto s : thread_segments[ tid ] ) {
                assert( s > 1 ); // 0th segments + 1th are for globals
                segments_to_tid[ s ] = tid;
            }
        }
    }

    void writeData( char * &mem ) const
    {
        blobWrite( mem, thread_segments, segments_in_stack, variablesFlags );
    }

    Value deref( const llvm::Value *v, int tid, bool prev ) const;

    void addSegment( int sid, const std::vector< int > &bws )
    {
        variablesFlags.emplace( variablesFlags.begin() + sid, bws.size(), F_DEFAULT );
    }

    void eraseSegment( int sid )
    {
        variablesFlags.erase( variablesFlags.begin() + sid );
    }

    bool isMultival( Value v ) const
    {
        if ( v.type == Value::Type::Constant )
            return false;
        assert( v.variable.segmentId < variablesFlags.size() );
        assert( v.variable.offset < variablesFlags[ v.variable.segmentId ].size() );
        return (variablesFlags[ v.variable.segmentId ][ v.variable.offset ] & F_MULTIVAL) == F_MULTIVAL;
    }

    void setMultival( VariableId variable, bool value )
    {
        assert( variable.segmentId < variablesFlags.size() );
        assert( variable.offset < variablesFlags[ variable.segmentId ].size() );

        char mask = F_MULTIVAL;
        if ( value )
            variablesFlags[ variable.segmentId ][ variable.offset ] |= mask;
        else
            variablesFlags[ variable.segmentId ][ variable.offset ] &= ~mask;
    }

    void setMultival( Value v, bool value )
    {
        assert( v.type == Value::Type::Variable );

        setMultival( v.variable, value );
    }

    void startThread()
    {
        thread_segments.emplace_back();
        segments_in_stack.emplace_back();
        newStack( thread_segments.size() - 1 );
        newSegment( thread_segments.size() - 1 );
    }
    
    // stack allocation
    int newSegment( unsigned tid );

    void newStack( unsigned tid )
    {
        assert( tid < segments_in_stack.size() );
        segments_in_stack[ tid ].push_back( 0 );
    }

    std::pair< unsigned, unsigned >
        getLastStackSegmentRange( unsigned tid, bool prev = false ) const
    {
        assert( tid < thread_segments.size() );
        unsigned last, first;

        if ( prev ) {
            assert( thread_segments[tid].size() > segments_in_stack[tid].back() );
            last = thread_segments[tid][thread_segments[tid].size() - 1 - segments_in_stack[tid].back()];
            first = last - segments_in_stack[tid][segments_in_stack[tid].size()-2] + 1;
        } else {
            last = thread_segments[tid].back();
            first = last - segments_in_stack[tid].back() + 1;
        }
        assert( last <= thread_segments[tid].back() );
        assert( last >= first );
        return std::make_pair( first, last + 1 );
    }

    void dropLastStack( unsigned tid )
    {
        assert( tid < thread_segments.size() );
        std::pair< unsigned, unsigned > segments_range = getLastStackSegmentRange( tid );
        auto first = segments_to_tid.begin() + segments_range.first;
        auto last = segments_to_tid.begin() + segments_range.second;
        unsigned stack_width = last - first;
        segments_to_tid.erase( first, last );
        for ( auto &sids : thread_segments ) {
            for ( auto &s : sids ) {
                if ( s >= segments_range.second )
                    s -= stack_width;
            }
        }
        assert( thread_segments[ tid ].size() >= stack_width );
        thread_segments[ tid ].erase( thread_segments[ tid ].end() - stack_width, thread_segments[ tid ].end() );
        if ( thread_segments[ tid ].empty() ) {
            assert( segments_in_stack[ tid ].size() == 1 );
            thread_segments.erase( thread_segments.begin() + tid );
            segments_in_stack.erase( segments_in_stack.begin() + tid );
            for ( auto &t : segments_to_tid )
                if ( t > tid )
                    --t;
        } else {
            segments_in_stack[ tid ].pop_back();
            assert( !segments_in_stack[ tid ].empty() );
        }
    }


    void leave( unsigned tid );

    void switchBB( BB bb, unsigned tid )
    {
        assert( tid <= current_frames.size() );

        static std::map< llvm::BasicBlock*, std::shared_ptr< Frame > > cache;
        auto found_it = cache.find( bb.bb );

        if ( found_it == cache.end() )
            found_it = cache.insert(
                    std::make_pair( bb.bb, std::make_shared< Frame >( bb ) ) ).first;

        if ( tid == current_frames.size() ) {
            current_frames.push_back( found_it->second );
        } else {
            current_frames[ tid ] = found_it->second;
        }
    }

    size_t getSize() const
    {
        return representation_size( thread_segments, segments_in_stack, variablesFlags );
    }


    friend std::ostream & operator<<( std::ostream & o, const MemoryLayout &m );

};

}

