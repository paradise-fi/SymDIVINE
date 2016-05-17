#pragma once

#include <toolkit/utils.h>
#include <string>
#include <map>
#include <algorithm>
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

class FormulaeCapture
{
    std::map<std::string, z3::check_result> data;
public:   
    static FormulaeCapture& get() {
        static FormulaeCapture cap;
        return cap;
    }
    
    static void insert(std::string s, z3::check_result res) {
        std::replace(s.begin(), s.end(), '\n', ' ');
        get().data.insert({ s, res });
    }
    
    static void dump(std::ostream& o) {
        for (const auto& item : get().data) {
            o << item.first << "\t" << item.second << "\n";
        }
    }
};

std::ostream& operator<<( std::ostream &o, const Statistics &s );

