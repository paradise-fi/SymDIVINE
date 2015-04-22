#pragma once

#include <iostream>
#include <memory>
#include <vector>

#include "evaluator.h"
#include "datastore.h"
#include "blobing.h"
#include "smtdatastore.h"
#include "programutils/config.h"
#include "toolkit/z3cache.h"

using namespace llvm_sym;

template <class Store>
Reachability<Store>::Reachability(const std::string& name) : model_name(name) {

}

template <class Store>
void Reachability<Store>::run() {
    std::shared_ptr<BitCode> bc = std::make_shared<BitCode>(model_name);
    Evaluator<Store> eval(bc);
    Database<Blob, SMTStore, LinearCandidate< SMTStore, SMTSubseteq>, blobHash,
        blobEqual> knowns;

    std::vector<Blob> to_do;

    Blob initial(eval.getSize(), eval.getExplicitSize());
    eval.write(initial.mem);

    knowns.insert(initial);
    to_do.push_back(initial);

    bool error_found = false;
    while (!to_do.empty() && !error_found) {
        Blob b = to_do.back();
        to_do.pop_back();

        std::vector< Blob > successors;

        eval.read(b.mem);
        eval.advance([&]() {
            Blob newSucc(eval.getSize(), eval.getExplicitSize());
            eval.write(newSucc.mem);

            if (eval.isError()) {
                std::cout << "Error state:\n" << eval.toString() << "is reachable."
                    << std::endl;
                error_found = true;
            }

            if (knowns.insertCheck(newSucc)) {
                if (Config.verbose.isSet() || Config.vverbose.isSet()) {
                    static int succs_total = 0;
                    std::cerr << ++succs_total << " states so far.\n";
                }
                successors.push_back(newSucc);
            }
        });
        std::random_shuffle(successors.begin(), successors.end());
        for (const Blob &succ : successors)
            to_do.push_back(succ);
    }

    if (!error_found)
        std::cout << "Safe." << std::endl;

    if (Config.statistics.isSet()) {
        std::cout << knowns.size() << " states generated" << std::endl;
    }
}