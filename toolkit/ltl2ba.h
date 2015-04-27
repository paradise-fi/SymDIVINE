#pragma once

/**
 * @author Jan Mrázek
 */
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <boost/graph/graphviz.hpp>
#include "graph.h"
#include "../llvmsym/formula/rpn.h"

class LtlException : public std::runtime_error {
public:
    LtlException(const std::string& msg) : runtime_error(msg) { }
};

class SpotException : public std::runtime_error {
public:
    SpotException(const std::string& msg) : runtime_error(msg) { }
};

/**
 * Calls SPOT tool to convert LTL formula to BA, then reads its graphviz output
 * and using given translator for atomic propositions creates Graph of BA
 */
template <class Translator>
class Ltl2ba {
public:
    typedef size_t index_type;

    struct Accepting {
        Accepting(bool val = false) : acc(val) {}
        operator bool() { return acc; }
        bool acc;
    };

    struct AP {
        /**
        * Constructs atomic proposition from string. Translator functor T is
        * used for translation
        */
        template <class T>
        AP(T t, const std::string& ap = "") : ap(t(ap)) {}

        llvm_sym::Formula ap;
    };

    /**
     * Default constructor. Takes one argument, remaining args are pased to
     * translator
     */
    template <class...Args>
    Ltl2ba(const std::string& ltl, Args...args) : ap_translator(args...) {
        // Construct ba using external spot tool
        std::string query = "ltl2tgba --ba -f \"" + ltl + "\" 2>&1";
        FILE* pipe = popen(query.c_str(), "r");
        if (pipe < 0)
            throw LtlException(strerror(errno));
        std::string dot;
        char c;
        while ((c = fgetc(pipe)) != EOF)
            dot.push_back(c);
        int res = pclose(pipe);
        if (res != 0)
            throw SpotException(dot);

        std::cout << dot << std::endl;

        // Convert graphviz to graph
        typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
            DotVertex, DotEdge> graph_t;

        graph_t graphviz;
        boost::dynamic_properties dp(boost::ignore_other_properties);

        dp.property("label", boost::get(&DotVertex::name, graphviz));
        dp.property("peripheries", boost::get(&DotVertex::peripheries, graphviz));
        dp.property("label", boost::get(&DotEdge::label, graphviz));

        bool status = boost::read_graphviz(dot, graphviz, dp);

        // Convert BA to Graph class
        typedef typename boost::property_map<graph_t, boost::vertex_index_t>::type IndexMap;
        IndexMap index = get(boost::vertex_index, graphviz);

        auto vert = boost::vertices(graphviz);
        for (; vert.first != vert.second; vert.first++) {
            index_type id = index[*vert.first];
            const std::string& label = graphviz[*vert.first].name;
            int count = graphviz[*vert.first].peripheries;

            if (label.empty()) // Initial state
                start_point = id;
            ba.add_vertex(id, count == 2);

            std::cout << id << ": " << label << ", " << count << std::endl;
        }

        auto edg = boost::edges(graphviz);
        for (; edg.first != edg.second; edg.first++) {
            index_type from = index[source(*edg.first, graphviz)];
            index_type to = index[target(*edg.first, graphviz)];
            const std::string& label = graphviz[*edg.first].label;

            ba.add_edge(from, to,ap_translator, label);

            std::cout << "(" << from << ", " << to << "): " << label << std::endl;
        }
    }

    index_type get_init_vert() {
        return start_point;
    }

    const Graph<index_type, Accepting, AP>& get_ba() {
        return ba;
    }
private:

    struct DotVertex {
        DotVertex() : peripheries(0) {}
        std::string name;
        int peripheries;
    };

    struct DotEdge {
        std::string label;
    };

    Translator ap_translator;
    Graph<index_type, Accepting, AP> ba;
    index_type start_point;
};

// Dummy interface for translating atomic proposition
class DummyTranslator {
public:
    llvm_sym::Formula operator()(const std::string&) {
        return llvm_sym::Formula();
    }
};