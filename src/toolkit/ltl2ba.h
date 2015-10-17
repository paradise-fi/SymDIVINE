#pragma once

/**
 * @author Jan Mrázek
 */
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <boost/graph/graphviz.hpp>
#include <algorithm>
#include "graph.h"
#include <llvmsym/formula/rpn.h>
#include <llvmsym/programutils/config.h>

class LtlConvException : public std::runtime_error {
public:
    LtlConvException(const std::string& msg) : runtime_error(msg) { }
};

class SpotException : public std::runtime_error {
public:
    SpotException(const std::string& msg) : runtime_error(msg) { }
};

class TranslateException : public std::runtime_error {
public:
	TranslateException(const std::string& msg) : runtime_error(msg) {}
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
        operator bool() const { return acc; }
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
	    // Preprocess formula
	    std::string formula = ap_translator.preprocess(ltl);
        // Construct ba using external spot tool
        std::string query = "ltl2tgba --ba -f \"" + formula + "\" 2>&1";
        FILE* pipe = popen(query.c_str(), "r");
        if (pipe < 0)
            throw LtlConvException(strerror(errno));
        std::string dot;
        char c;
        while ((c = fgetc(pipe)) != EOF)
            dot.push_back(c);
        int res = pclose(pipe);
        if (res != 0)
            throw SpotException(dot);

        if (Config.is_set("--vverbose"))
            std::cout << "Spot output:\n" << dot << std::endl;

        // Convert graphviz to graph
        typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
            DotVertex, DotEdge> graph_t;

        graph_t graphviz;
        boost::dynamic_properties dp(boost::ignore_other_properties);

        dp.property("label", boost::get(&DotVertex::name, graphviz));
        dp.property("peripheries", boost::get(&DotVertex::peripheries, graphviz));
        dp.property("label", boost::get(&DotEdge::label, graphviz));

        bool status = boost::read_graphviz(dot, graphviz, dp);
        if (!status)
            throw LtlConvException("Cannot read ltl2ba output");

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

            if (Config.is_set("--vverbose"))
                std::cout << id << ": " << label << ", " << count << std::endl;
        }

        auto edg = boost::edges(graphviz);
        for (; edg.first != edg.second; edg.first++) {
            index_type from = index[source(*edg.first, graphviz)];
            index_type to = index[target(*edg.first, graphviz)];
            const std::string& label = graphviz[*edg.first].label;

            ba.add_edge(from, to, ap_translator, label);

            if (Config.is_set("--vverbose"))
                std::cout << "(" << from << ", " << to << "): " << label << std::endl;
        }
    }

    index_type get_init_vert() const {
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
	std::string preprocess(const std::string& formula) {
		return formula;
	}

    llvm_sym::Formula operator()(const std::string&) {
        return llvm_sym::Formula();
    }
};

extern std::pair<std::vector<std::shared_ptr<llvm_sym::Formula>>, std::string>
    parse_ltl(const std::string& formula, bool verbose, bool vverbose);
extern int yyparse(void);
extern std::vector<std::shared_ptr<llvm_sym::Formula>> formulas;
extern std::string res_formula;

class LtlTranslator {
public:
	/**
	 * Takes formula, abstracts atomic propositions and returns modified
	 * formula with proper substitution.
	 */
	std::string preprocess(const std::string& formula) {
		auto res = parse_ltl(formula, Config.is_set("--verbose"), Config.is_set("--vverbose"));

		aps = res.first;
		return res.second;
	}

	// Takes name of atomic proposition and returns its Formula representation.
	llvm_sym::Formula operator()(std::string ap) {
    	// Remove white chars
    	auto new_end = std::remove_if(ap.begin(), ap.end(), ::isspace);
    	ap.erase(new_end, ap.end());
		// Try to find operator &&
    	size_t and_pos = ap.find("&&");
    	// Try to find operator ||
    	size_t or_pos = ap.find("||");

    	if (and_pos != std::string::npos) {
        	std::string op1(ap.begin(), ap.begin() + and_pos);
        	std::string op2(ap.begin() + 2 + and_pos, ap.end());
        	return translate_ap(op1) && translate_ap(op2);
    	}

    	if (or_pos != std::string::npos) {
        	std::string op1(ap.begin(), ap.begin() + or_pos);
        	std::string op2(ap.begin() + 2 + or_pos, ap.end());
        	return translate_ap(op1) || translate_ap(op2);
    	}

    	return translate_ap(ap);
	}
private:
	std::vector<std::shared_ptr<llvm_sym::Formula>> aps; // List of atomic propositions

    /**
     * Translates given ap to its formula representation
     */
    llvm_sym::Formula translate_ap(const std::string& ap) {
        if (ap.empty() || ap[0] == '1')
            return llvm_sym::Formula::buildBoolVal(true);

        if ((ap[0] != 'a' || ap[1] != 'p') && (ap[0] != '!' || ap[1] != 'a' || ap[2] != 'p')) {
            throw TranslateException("Invalid AP name (missing \"ap\" in name): " + ap);
        }

        bool negated = ap[0] == '!';
        size_t offset = negated ? 3 : 2;

        std::istringstream in(std::string(ap.begin() + offset, ap.end()));
        size_t number;
        if (!(in >> number) || number > ap.size()) {
            throw TranslateException("Cannot translate AP (invalid number): " + ap);
        }

        if (negated)
            return !(*(aps[number - 1]));
        return *(aps[number - 1]);
    }
};