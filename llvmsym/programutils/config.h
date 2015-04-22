#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <tclap/CmdLine.h>

struct ConfigStruct {
    ConfigStruct() :
        cmd("SymDiVine", ' ', "0.0"),
        reachability("", "reachability", "Checks model for safety properties", true),
        ltl("", "ltl", "Checks model for LTL property", false),
        statistics("s", "statistics", "Prints statistical info", false),
        verbose("v", "verbose", "Enables verbose mode", false),
        vverbose("w", "vverbose", "Enables advanced verbose mode", false),
        dontsimplify("", "dont-simplify", "Disables simplification of formulas", false),
        cheapsimplify("", "cheap-simplify", "Enables only cheap simplification of formulas", false),
        enablecaching("c", "enable-caching", "Enables caching of formulas", false),
        disabletimout("", "disable-timout", "Disables Z3 timeout", false),
        model("model", "LLVM model for verification", true, "", "Input model in ll format"),
        ltl_prop("", "ltlspec", "file with LTL formula", false, "", "Input LTL formula")
    {
        cmd.add(statistics);
        cmd.add(verbose);
        cmd.add(vverbose);
        cmd.add(dontsimplify);
        cmd.add(cheapsimplify);
        cmd.add(enablecaching);
        cmd.add(model);

        cmd.add(reachability);
        cmd.add(ltl);
    }

    /**
     * Parses cmd-line arguments
     */
    void parse_cmd_args(int argc, char* argv[]) {
        cmd.parse(argc, argv);
    }

private:
    TCLAP::CmdLine cmd;
public:

    /* parameters */
    TCLAP::SwitchArg reachability;
    TCLAP::SwitchArg ltl;

    TCLAP::SwitchArg statistics;
    TCLAP::SwitchArg verbose;
    TCLAP::SwitchArg vverbose;

    TCLAP::SwitchArg dontsimplify;
    TCLAP::SwitchArg cheapsimplify;
    TCLAP::SwitchArg enablecaching;
    TCLAP::SwitchArg disabletimout;

    TCLAP::UnlabeledValueArg<std::string> model;
    TCLAP::ValueArg<std::string> ltl_prop;
};

// Global instance of Config
extern ConfigStruct Config;

