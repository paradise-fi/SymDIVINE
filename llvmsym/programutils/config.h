#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <docopt/docopt.h>

static const char USAGE[] =
R"(SymDiVine.
    Usage:
      naval_fate ship new <name>...
      naval_fate ship <name> move <x> <y> [--speed=<kn>]
      naval_fate ship shoot <x> <y>
      naval_fate mine (set|remove) <x> <y> [--moored | --drifting]
      naval_fate (-h | --help)
      naval_fate --version
    Options:
      -h --help     Show this screen.
      --version     Show version.
      --speed=<kn>  Speed in knots [default: 10].
      --moored      Moored (anchored) mine.
      --drifting    Drifting mine.
)";

class ArgNotFoundException : std::runtime_error {
public:
    ArgNotFoundException(const std::string& msg) : runtime_error(msg) {}
};

class ArgTypeException : std::runtime_error {
public:
    ArgTypeException(const std::string& msg) : runtime_error(msg) {}
};

struct ConfigStruct {
    /**
     * Parses cmd-line arguments
     */
    void parse_cmd_args(int argc, char* argv[]) {
        args = docopt::docopt(
                USAGE,
                { argv + 1, argv + argc },
                true              // show help if requested
            );
    }

    bool is_set(const std::string& name) {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isBool())
            throw ArgTypeException(name);
        return res->second.asBool();
    }

    long get_long(const std::string& name) {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isLong())
            throw ArgTypeException(name);
        return res->second.asLong();
    }

    std::string get_string(const std::string& name) {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isString())
            throw ArgTypeException(name);
        return res->second.asString();
    }

    std::vector<std::string> get_strings(const std::string& name) {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isStringList())
            throw ArgTypeException(name);
        return res->second.asStringList();
    }

private:
    std::map<std::string, docopt::value> args;
};

// Global instance of Config
extern ConfigStruct Config;

