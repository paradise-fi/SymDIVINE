#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>

class Config {
    struct CmdParameterValue {
        std::string name;
        char short_option;
    };

    std::map< std::string, bool > options;
    std::map< std::string, std::string > CmdLongParameters;
    std::map< char, std::string > CmdShortParameters;
    std::string argument;

    public:

    static Config& get()
    {
        static Config _config = Config();
        return _config;
    }

    static void setOption( const std::string name, bool value = true )
    {
        get().options[ name ] = value;
    }

    static bool getOption( const std::string name )
    {
        assert( get().options.count( name ) );
        return get().options[ name ];
    }

    static std::string getArgument()
    {
        return get().argument;
    }

    static void addArgOption( const std::string long_option, const std::string name, char option = 0 )
    {
        get().CmdLongParameters[ long_option ] = name;
        if ( option )
            get().CmdShortParameters[ option ] = name;
    }

    static void parseArgs( int args, char **argv )
    {
        for ( auto opt : get().CmdLongParameters ) {
            setOption( opt.second, false );
        }
        for ( auto opt : get().CmdShortParameters ) {
            setOption( opt.second, false );
        }

        for ( int i = 1; i < args; ++i ) {
            char *arg = argv[ i ];
            if ( strlen( arg ) == 2 && arg[0] == '-' ) {
                char option = arg[1];
                if ( get().CmdShortParameters.count( option ) == 0 ) {
                    std::cerr << "unknown option " << arg << std::endl;
                    abort();
                } else {
                    get().setOption( get().CmdShortParameters[ option ] );
                }
            } else if ( strlen( arg ) >= 3 && arg[0] == '-' && arg[1] == '-' ) {
                std::string option = std::string( arg+2 );
                if ( get().CmdLongParameters.count( option ) == 0 ) {
                    std::cerr << "unknown option " << arg << std::endl;
                    abort();
                } else {
                    get().setOption( get().CmdLongParameters[ option ] );
                }
            } else {
                if ( !get().argument.empty() ) {
                    std::cerr << "Got two program names: \"" << get().argument << "\" and \""
                              << arg << "\". Aborting." << std::endl;
                    abort();
                }
                get().argument = std::string( arg );
            }
        }
    }


    protected:
    Config()
    {
        CmdLongParameters[ "dont-simplify" ] = "dontsimplify";
        CmdLongParameters[ "cheap-simplify" ] = "cheapsimplify";
        CmdLongParameters[ "statistics" ] = "statistics";
        CmdLongParameters[ "verbose" ] = "verbose";
        CmdLongParameters[ "vverbose" ] = "vverbose";

        CmdShortParameters[ 's' ] = "statistics";
        CmdShortParameters[ 'v' ] = "verbose";

        options[ "dontsimplify" ] = false;
        options[ "cheapsimplify" ] = false;
        options[ "verbose" ] = false;
        options[ "vverbose" ] = false;
        options[ "statistics" ] = false;
    }
};

