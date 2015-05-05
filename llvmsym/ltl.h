#pragma once
#include <string>
#include <stack>
#include "evaluator.h"
#include "datastore.h"
#include "blobing.h"
#include "../toolkit/ltl2ba.h"

class LtlException : public std::runtime_error {
public:
    LtlException(const std::string& msg) : std::runtime_error(msg) { }
};

/**
 * Checks if given LTL property of a model holds or not
 */
template <class Store, class Hit>
class Ltl {
public:
    Ltl(const std::string& model, const std::string& prop);
    void run();
private:
    typedef typename Ltl2ba<DummyTranslator>::index_type index_type;
    enum class VertexColor { WHITE, GRAY, BLACK };
    struct VertexInfo {
        VertexInfo(VertexColor outer = VertexColor::WHITE,
                   VertexColor inner = VertexColor::WHITE)
            : outer_color(outer), inner_color(inner) {}

        VertexColor outer_color;
        VertexColor inner_color;
    };

    std::string model_name; // Name of the file with bitcode
    Ltl2ba<LtlTranslator> ba; // Buchi automaton for given property
    Database<Blob, Store, LinearCandidate<Store, Hit>, blobHashExplicitUserPart,
        blobEqualExplicitUserPart> knowns; // Database of the states
    Graph<StateId, VertexInfo> graph; // Graph of the state space
        // Id is create from StateId and state of BA

    /**
     * Runs nested DFS with given initial state
     */
    void run_nested_dfs(Evaluator<Store>& eval, StateId start_vertex);
};

#include "ltl.tpp"