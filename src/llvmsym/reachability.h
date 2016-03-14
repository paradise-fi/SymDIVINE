#pragma once
#include <string>
#include <queue>

#include "evaluator.h"
#include "datastore.h"
#include "blobing.h"
#include "smtdatastore.h"
#include "smtdatastore_partial.h"
#include "programutils/config.h"
#include "../toolkit/graph.h"

using namespace llvm_sym; // This is weird, can't compile with direct usage of namespace

/**
 * Reachability of bad states on given model
 */
template <class Store, class Hit>
class Reachability {
public:
    Reachability(const std::string& model);
    void run();
    void output_state_space(const std::string& filename);
private:
    Evaluator<Store> eval; // Evaluator for the bitcode
    Database<Blob, Store, LinearCandidate<Store, Hit>, blobHashExplicitPart,
        blobEqualExplicitPart> knowns;
    Graph<StateId> graph;
};

#include "reachability.tpp"