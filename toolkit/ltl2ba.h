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

template <class Translator>
class Ltl2ba {
public:
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

        // Only testing of retrieval the graph from boost::adjacency_list
        typedef typename boost::property_map<graph_t, boost::vertex_index_t>::type IndexMap;
        IndexMap index = get(boost::vertex_index, graphviz);

        auto vert = boost::vertices(graphviz);
        for (; vert.first != vert.second; vert.first++) {
            std::cout << index[*vert.first] << ": "
                << graphviz[*vert.first].name << ", "
                << graphviz[*vert.first].peripheries
                << std::endl;
        }

        auto edg = boost::edges(graphviz);
        for (; edg.first != edg.second; edg.first++) {
            std::cout << "(" << index[source(*edg.first, graphviz)] << ", "
                << index[target(*edg.first, graphviz)] << "): ";
            std::cout << graphviz[*edg.first].label << std::endl;
        }
    }
private:
    struct Accepting {
        Accepting(bool val = false) : acc(val) {}
        operator bool() { return acc; }
        bool acc;
    };

    struct AP {
        AP(const std::string& ap = "") : ap(ap) {}
        std::string ap;
    };

    struct DotVertex {
        DotVertex() : peripheries(0) {}
        std::string name;
        int peripheries;
    };

    struct DotEdge {
        std::string label;
    };

    Translator ap_translator;
    Graph<size_t, Accepting, AP> ba;
};