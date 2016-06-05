#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <docopt/docopt.h>
#include <thread>

extern const char USAGE[];

class ArgNotFoundException : public std::runtime_error {
public:
    ArgNotFoundException(const std::string& msg) : runtime_error(msg) {}
};

class ArgTypeException : public std::runtime_error {
public:
    ArgTypeException(const std::string& msg) : runtime_error(msg) {}
};

struct ConfigStruct {
    /**
     * Parses cmd-line arguments
     */
    void parse_cmd_args(int argc, char* argv[]) {
        // There is a possible memory corruption in docopt
        // resulting in incorrect results of SymDIVINE.
        // Keeping docopt parsing in separate thread is a
        // temporary hack. Fix it! ToDo
        std::thread t([&]() {
            args = docopt::docopt(std::string(USAGE), { argv + 1, argv + argc }, true);
        });
        t.join();
    }
    
    void dump(std::ostream& o = std::cout) const {
        for (auto const& arg : args) {
            o << arg.first << ": " << arg.second << std::endl;
        }
    }

    bool is_set(const std::string& name) const {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isBool())
            return (bool)res->second; 
        return res->second.asBool();
    }

    long get_long(const std::string& name) const {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isLong()) {
            if (res->second.isString()) {
                try {
                    return std::stoi(res->second.asString());
                } 
                catch (std::invalid_argument&) { }
            }   
            throw ArgTypeException(name);
        }
        return res->second.asLong();
    }

    std::string get_string(const std::string& name) const {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isString())
            throw ArgTypeException(name);
        return res->second.asString();
    }

    std::vector<std::string> get_strings(const std::string& name) const {
        auto res = args.find(name);
        if (res == args.end())
            throw ArgNotFoundException(name);
        if (!res->second.isStringList())
            throw ArgTypeException(name);
        return res->second.asStringList();
    }

    /**
     * Sets an value for argument (useful for testing)
     */
    template <class T>
    void set(const std::string& name, T val) {
        args[name] = docopt::value(val);
    }

private:
    std::map<std::string, docopt::value> args;
};

// Global instance of Config
extern ConfigStruct Config;

