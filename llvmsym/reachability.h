#pragma once
#include <string>

#include "evaluator.h"
#include "datastore.h"
#include "blobing.h"
#include "smtdatastore.h"
#include "programutils/config.h"

using namespace llvm_sym; // This is weird, can't compile with direct usage of namespace

/**
 * Reachability of bad states on given model
 */
template <class Store, class Hit>
class Reachability {
public:
    Reachability(const std::string& model);
    void run();
private:
    std::string model_name;
    Database<Blob, Store, LinearCandidate<Store, Hit>, blobHash,
        blobEqual> knowns;
};

#include "reachability.tpp"