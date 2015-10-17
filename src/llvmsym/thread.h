#pragma once
#include <llvmsym/blobutils.h>
#include <cassert>
#include <ostream>
#include <boost/iterator/counting_iterator.hpp>

namespace llvm_sym {

struct Control {
    struct PC {
        short unsigned function;
        short unsigned basicblock;
        short unsigned instruction;

      bool operator==( const PC &pc ) const {
        if ( function != pc.function ||
             basicblock != pc.basicblock ||
             instruction != pc.instruction )
          return false;
        else
          return true;
      }
    };

    std::vector<std::vector<PC>> context;
    std::vector<PC> previous_bb;
    std::vector<short unsigned> tids;
    std::vector<size_t> atomic_section; // Idx of threads
    short unsigned next_free_tid = 0;

    void writeData( char * &mem ) const
    {
        blobWrite( mem, context, previous_bb, tids, next_free_tid );
    }

    void readData( const char * &mem )
    {
        context.clear();
        blobRead( mem, context, previous_bb, tids, next_free_tid );

        assert( context.size() == previous_bb.size() && context.size() == tids.size() );
    }

    bool hasTid( int tid ) const
    {
        for ( int t : tids )
            if ( tid == t )
                return true;
        return false;
    }

    void clear()
    {
        context.clear();
    }

    size_t getSize() const
    {
        return representation_size( context, previous_bb, tids, next_free_tid );
    }

    // returns tid
    int startThread(short unsigned function_no)
    {
        PC func_pc;
        func_pc.function = function_no;
        func_pc.basicblock = 0;
        func_pc.instruction = 0;

        context.push_back( std::vector< PC >( 1, func_pc ) );
        previous_bb.push_back( PC() );
        tids.push_back( next_free_tid++ );
        return tids.back();
    }

    void enterFunction(short unsigned function_no, size_t tid)
    {
        PC new_pc;
        new_pc.function = function_no;
        new_pc.basicblock = new_pc.instruction = 0;
    
        context[tid].push_back( new_pc );
    }

    void jumpTo(PC new_pc, size_t tid, bool remember_previous_bb)
    {
        assert( tid < context.size() );
        assert( !context[tid].empty() );
        previous_bb[ tid ] = remember_previous_bb ? context[tid].back() : PC();
        context[tid].back() = new_pc;
    }

    bool leave(size_t tid)
    {
        assert( tid < context.size() );
        assert( !context[tid].empty() );

        context[tid].pop_back();

        if ( context[ tid ].size() == 0 ) {
            context.erase( context.begin() + tid );
            previous_bb.erase( previous_bb.begin() + tid );
            tids.erase( tids.begin() + tid );
            return true;
        } else {
            return false;
        }
    }

    size_t threadCount() const
    {
        return context.size();
    }

    void advance(size_t tid, int relative = 1)
    {
        assert( tid < context.size() );
        context[ tid ].back().instruction += relative;
    }

    const PC& getPC(size_t tid) const
    {
        assert( tid < context.size() );
        return context[ tid ].back();
    }
    
    PC &getPC(size_t tid)
    {
        assert( tid < context.size() );
        return context[ tid ].back();
    }

    const PC& getPrevPC(size_t tid) const
    {
        assert( tid < context.size() );
        assert( context[ tid ].size() > 1 );

        return context[ tid ][ context[tid].size() - 2 ];
    }

    bool last(size_t tid) const
    {
        assert( tid < context.size() );
        return context[ tid ].size() == 1;
    }
    
    void enter_atomic_section(size_t tid) {
        assert(tid < context.size());
        atomic_section.push_back(tid);
    }
    
    void leave_atomic_section(size_t tid) {
        assert(!atomic_section.empty() && atomic_section.back() == tid);
        atomic_section.pop_back();
    }
    
    std::vector<size_t> get_allowed_threads() const {
        if (atomic_section.empty()) {
            // Allow all threads
            return std::vector<size_t>(
                boost::counting_iterator<size_t>(0),
                boost::counting_iterator<size_t>(context.size())
                );
        }
        // Allow only single thread
        return std::vector<size_t>({ atomic_section.back() });
    }
};

static_assert( std::is_pod< Control::PC >::value, "Control::PC must be POD" );

std::ostream & operator<<( std::ostream &o, const Control &c );

}

