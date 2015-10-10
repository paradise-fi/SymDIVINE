#include <cxxabi.h>
#include <string>

struct Demangler {
    static std::string demangle( const std::string what )
    {
        int     status;
        char   *realname;

        realname = abi::__cxa_demangle( what.c_str(), 0, 0, &status );

        if ( !realname )
            return what;
        else {
            for ( char *p = realname; *p != 0; ++p )
                if ( *p == '(' )
                    *p = 0;
            std::string result = realname;
            free(realname);
            return result;
        }
    }
};
