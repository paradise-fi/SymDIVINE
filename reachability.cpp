#include "llvmsym/evaluator.h"
#include "llvmsym/datastore.h"
#include "llvmsym/blobing.h"
#include "llvmsym/smtdatastore.h"
#include "llvmsym/programutils/config.h"

#include <iostream>
#include <memory>
#include "toolkit/z3cache.h"
//#include <set>
#include <vector>
//#include <unordered_set>

using namespace llvm_sym;

using namespace std;

typedef Evaluator< SMTStore > Eval;

void found( const Eval &eval )
{
    std::cout << "Error state:\n" << eval.toString() << "is reachable."
        << std::endl;
}

int main( int args, char *argv[] )
{
    Config.parse_cmd_args(args, argv);

    /*if ( args < 2 ) {
        cerr << "Syntax:\n\t" << argv[0] << " input.ll" << std::endl;
        return 0;
    }*/

    shared_ptr< BitCode > bc = make_shared< BitCode >( Config.model.getValue() );
    Eval eval( bc );
    Database< Blob, SMTStore, LinearCandidate< SMTStore, SMTSubseteq >, blobHash,
              blobEqual > knowns;

    vector< Blob > to_do;

    Blob initial( eval.getSize(), eval.getExplicitSize() );
    eval.write( initial.mem );

    knowns.insert( initial );
    to_do.push_back( initial );

    bool error_found = false;
    while ( !to_do.empty() && !error_found ) {
        Blob b = to_do.back();
        to_do.pop_back();

        std::vector< Blob > successors;

        eval.read( b.mem );
        eval.advance( [&]() {
                Blob newSucc( eval.getSize(), eval.getExplicitSize() );
                eval.write( newSucc.mem );

                if ( eval.isError() ) {
                    found( eval );
                    error_found = true;
                }

                if ( knowns.insertCheck( newSucc ) ) {
                    if ( Config.verbose.isSet() || Config.vverbose.isSet() ) {
                        static int succs_total = 0;
                        std::cerr << ++succs_total << " states so far.\n";
                    }
                    successors.push_back( newSucc );
                }
        } );
        std::random_shuffle( successors.begin(), successors.end() );
        for ( const Blob &succ : successors )
            to_do.push_back( succ );
    }

    if ( !error_found )
        std::cout << "Safe." << std::endl;

    if (Config.statistics.isSet()) {
        cout << knowns.size() << endl;
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
    std::cout << knowns.size() << " states generated.\n";
}

