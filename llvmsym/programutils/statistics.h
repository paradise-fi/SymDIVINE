#pragma once

#include <string>
#include <map>
#include <cassert>
#include <ostream>

class Statistics {
    std::map< std::string, int > data;

    public:

    static Statistics& get()
    {
        static Statistics _stats;
        return _stats;
    }

    static void createCounter( std::string name, int initial_value = 0 )
    {
        assert( get().data.find( name ) == get().data.end() );
        get().data.insert( make_pair( name, initial_value ) );
    }

    static int &getCounter( std::string name )
    {
        return get().data[ name ];
    }

    friend std::ostream& operator<<( std::ostream &o, const Statistics &s );

    protected:

    Statistics() = default;
};

std::ostream& operator<<( std::ostream &o, const Statistics &s );

