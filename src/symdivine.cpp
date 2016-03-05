#include "llvmsym/programutils/config.h"
#include <iostream>

#include "toolkit/z3cache.h"
#include "llvmsym/reachability.h"
#include "llvmsym/ltl.h"

int main(int args, char *argv[])
{
    try {
        Config.parse_cmd_args(args, argv);
        
        if (Config.is_set("--version")) {
            std::cout << "SymDIVINE v0.2\n";
            return 0;
        }

        if (Config.is_set("reachability")) {
            if (Config.is_set("--partialstore")) {
                Reachability<SMTStorePartial, SMTSubseteq<SMTStorePartial>>
                    reachability(Config.get_string("<model>"));
                reachability.run();
            }
            else {
                Reachability<SMTStore, SMTSubseteq<SMTStore>>
                    reachability(Config.get_string("<model>"));
                reachability.run();
            }
            /**
             * ToDo: Add outputting of statistic data ec.
             */
        }

        if (Config.is_set("ltl")) {
            Ltl<SMTStore, SMTSubseteq<SMTStore>>
                ltl(Config.get_string("<model>"), Config.get_string("<property>"),
                    Config.is_set("--iterative"));
            ltl.run();
            /**
             * ToDo: Add outputting of statistic data ec.
             */
            ltl.output_state_space("output.dot");
        }

        if (Config.is_set("--statistics")) {
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
    catch (const ArgNotFoundException& e) {
        std::cerr << "Missing command line argument " << e.what() << "\n";
        return 1;
    }
    catch (const ArgTypeException& e) {
        std::cerr << "Wrong type of command line argument " << e.what() << "\n";
        return 1;
    }
    /*catch (const z3::exception& e) {
        std::cerr << "Z3 error: " << e.msg() << "\n";
    }*/
    return 0;
}

