#include <llvmsym/programutils/statistics.h>
#include <iomanip>
#include <algorithm>

std::ostream& operator<<( std::ostream &o, const Statistics &s )
{
    o << "General statistics\n"
         "------------------\n";

    unsigned long l_max_width = 0, r_max_width = 0;

    for ( auto counter : s.data ) {
        l_max_width = std::max( l_max_width, counter.first.length() );
        r_max_width = std::max( r_max_width, std::to_string( counter.second ).length() );
    }

    for ( auto counter : s.data ) {
        o << std::left << std::setw( l_max_width + 1 ) << counter.first << ": "
          << std::right << std::setw( r_max_width + 1 ) << counter.second << std::endl;
    }

    return o;
}

