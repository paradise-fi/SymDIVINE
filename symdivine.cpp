#include "llvmsym/programutils/config.h"
#include <iostream>

#include "toolkit/z3cache.h"
#include "llvmsym/reachability.h"
#include "llvmsym/ltl.h"

int main( int args, char *argv[] )
{
    Config.parse_cmd_args(args, argv);

    if (Config.reachability.isSet()) {
        Reachability<SMTStore, SMTSubseteq> reachability(Config.model.getValue());
        reachability.run();
        /**
         * ToDo: Add outputting of statistic data ec.
         */
    }

    if (Config.ltl.isSet()) {
        Ltl<SMTStore> ltl(Config.model.getValue(), Config.ltl_prop.getValue());
        ltl.run();
        /**
         * ToDo: Add outputting of statistic data ec.
         */
    }

    if (Config.statistics.isSet()) {
        std::cout << Statistics::get();
        std::cout << "\n";
        Z3cache.dump_stat(std::cout);
        std::cout << "\n";

        size_t time = 0;
        Z3cache.process(
            [&time](const Z3SubsetCall&, z3::check_result, const Z3Info& i) {
            time += i.time * i.accessed;
        });
        std::cout << "Time saved: " << time << " us\n";
    }
}

