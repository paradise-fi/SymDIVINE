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
    Ltl(const std::string& model, const std::string& prop, bool depth_bound = false);
    void run();
    void output_state_space(const std::string& filename);
private:
    typedef typename Ltl2ba<DummyTranslator>::index_type index_type;
    enum class VertexColor { WHITE, GRAY, BLACK };
    struct VertexInfo {
        VertexInfo(VertexColor outer = VertexColor::WHITE,
                   VertexColor inner = VertexColor::WHITE,
                   size_t depth = 0)
            : outer_color(outer), inner_color(inner), depth(depth) {}
        
        void reset() {
            outer_color = inner_color = VertexColor::WHITE;
        }

        VertexColor outer_color;
        VertexColor inner_color;
        size_t      depth;
    };

    Evaluator<Store> eval; // Evaluator for the bitcode
    bool depth_bounded; // Use iterative DFS?
    Ltl2ba<LtlTranslator> ba; // Buchi automaton for given property
    Database<Blob, Store, LinearCandidate<Store, Hit>, blobHashExplicitUserPart,
        blobEqualExplicitUserPart> knowns; // Database of the states
    Graph<StateId, VertexInfo> graph; // Graph of the state space

    /**
     * Runs nested DFS with given initial state
     */
    void run_nested_dfs(StateId start_vertex);
    
    /**
     * Resets DFS state, so next iteration can be started
     */
    void reset_dfs();

    /**
     * Run inner pass of nested DFS
     * @return true if accepting cycle is found
     */
    bool run_inner_dfs(StateId start_vertex);
    
    /**
     * Generate successors of given vertex, insert them into knowns and
     * return their Ids
     */
    std::vector<StateId> generate_successors(StateId vertex_id);
};

#include "ltl.tpp"