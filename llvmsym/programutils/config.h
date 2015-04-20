#pragma once

#include <map>
#include <string>
#include <cstring>
#include <cassert>
#include <iostream>
#include <tclap/CmdLine.h>

struct ConfigStruct {
    ConfigStruct() :
        statistics("s", "statistics", "Prints statistical info", false),
        verbose("v", "verbose", "Enables verbose mode", false),
        vverbose("w", "vverbose", "Enables advanced verbose mode", false),
        dontsimplify("", "dont-simplify", "Disables simplification of formulas", false),
        cheapsimplify("", "cheap-simplify", "Enables only cheap simplification of formulas", false),
        enablecaching("c", "enable-caching", "Enables caching of formulas", false),
        disabletimout("", "disable-timout", "Disables Z3 timeout", false),
        model("model", "LLVM model for verification", true, "", "Input model in ll format"),
        cmd("SymDiVine", ' ', "Vojta")
    {
        cmd.add(statistics);
        cmd.add(verbose);
        cmd.add(vverbose);
        cmd.add(dontsimplify);
        cmd.add(cheapsimplify);
        cmd.add(enablecaching);
        cmd.add(model);
    }

    /**
     * Parses cmd-line arguments
     */
    void parse_cmd_args(int argc, char* argv[]) {
        cmd.parse(argc, argv);
    }

    /* parameters */
    TCLAP::SwitchArg statistics;
    TCLAP::SwitchArg verbose;
    TCLAP::SwitchArg vverbose;
    TCLAP::SwitchArg dontsimplify;
    TCLAP::SwitchArg cheapsimplify;
    TCLAP::SwitchArg enablecaching;
    TCLAP::SwitchArg disabletimout;
    TCLAP::UnlabeledValueArg<std::string> model;
private:
    TCLAP::CmdLine cmd;
};

// Global instance of Config
extern ConfigStruct Config;

