#include <algorithm>
#include <functional>
#include <llvmsym/smtdatastore_partial.h>
#include <toolkit/z3cache.h>

namespace llvm_sym {

unsigned SMTStorePartial::unknown_instances = 0;
    
std::ostream& operator<<(std::ostream& o, const SMTStorePartial::dependency_group& g) {
    o << "Variables: ";
    for (const auto& ident : g.group)
        o << "seg_" << ident.seg << "_off_" << ident.off << ", ";
    o << "\nPath condition:\n";
    for (const auto& pc : g.path_condition)
        o << pc << "\n";
    o << "Definitions:\n";
    for (const auto& def : g.definitions)
        o << def.to_formula() << "\n";
    o << "\n";
    return o;
}

std::ostream & operator<<( std::ostream & o, const SMTStorePartial &v )
{
    o << "data:\n";
    int counter = 0;
    for (const auto& group : v.sym_data) {
        o << "Group " << counter << "\n";
        o << group.second;     
        counter++;
    }
        
    return o;
}

bool SMTStorePartial::empty() const
{
    ++Statistics::getCounter( STAT_EMPTY_CALLS );

    z3::context c;
    z3::expr pc( c );

    z3::solver s( c );
    for (const auto& group : sym_data) {
        for (const Definition &def : group.second.definitions)
            s.add(toz3(def.to_formula(), 'a', c));

        for (const Formula &pc : group.second.path_condition)
            s.add(toz3(pc, 'a', c));
    }

    ++Statistics::getCounter( STAT_SMT_CALLS );
    z3::check_result r = s.check();

    assert( r != z3::unknown );

    return r == z3::unsat;
}

bool SMTStorePartial::subseteq(
        const std::vector<std::reference_wrapper<const dependency_group>>& a_g,
        const std::vector<std::reference_wrapper<const dependency_group>>& b_g,
        const SMTStorePartial& aa,
        const SMTStorePartial& bb)
{
    if (a_g.empty() && b_g.empty())
        return true;
    // Merge dependencies
    dependency_group a_group;
    for (const auto& item : a_g)
        a_group.append(item.get());
    
    dependency_group b_group;
    for (const auto& item : b_g)
        b_group.append(item.get());
    
    ++Statistics::getCounter(STAT_SUBSETEQ_CALLS);
    if (a_group.definitions == b_group.definitions) {
        bool equal_syntax = a_group.path_condition.size() == b_group.path_condition.size();
        for (size_t i = 0; equal_syntax && i < a_group.path_condition.size(); ++i) {
            if (a_group.path_condition[i]._rpn != b_group.path_condition[i]._rpn)
                equal_syntax = false;
        }
        if (equal_syntax) {
            std::cout << "Equal syntax\n";
            ++Statistics::getCounter(STAT_SUBSETEQ_SYNTAX_EQUAL);
            return true;
        }
    }

    std::map< Formula::Ident, Formula::Ident > to_compare;
    for (unsigned s = 0; s < aa.generations.size(); ++s) {
        assert(aa.generations[s].size() == bb.generations[s].size());
        for (unsigned offset = 0; offset < aa.generations[s].size(); ++offset) {
            Value var;
            var.type = Value::Type::Variable;
            var.variable.segmentId = s;
            var.variable.offset = offset;

            Formula::Ident a_atom = aa.build_item(var);
            Formula::Ident b_atom = bb.build_item(var);

            if (!aa.depends_on(var) && !bb.depends_on(var))
                continue;

            to_compare.insert(std::make_pair(a_atom, b_atom));
        }
    }

    if (to_compare.empty())
        return true;
    std::vector<Formula::Ident> tmp;
    std::set_union(a_group.group.begin(), a_group.group.end(),
        b_group.group.begin(), b_group.group.end(),
        std::back_inserter(tmp));
    
    std::map<Formula::Ident, Formula::Ident> to_compare2;
    for (const auto& id : tmp) {
        to_compare2.insert(std::make_pair(id, id));
    }
    
    std::cout << "To compare:\n";
    for (const auto& ident : to_compare)
        std::cout << "s" << ident.first.seg << "o" << ident.first.off << "g" << ident.first.gen << "b" << (int)ident.first.bw << ",";
    std::cout << "\n";
    for (const auto& ident : to_compare)
        std::cout << "s" << ident.second.seg << "o" << ident.second.off << "g" << ident.second.gen << "b" << (int)ident.second.bw << ",";
    std::cout << "\n";
    std::cout << "To compare2:\n";
    for (const auto& ident : to_compare2)
        std::cout << "s" << ident.first.seg << "o" << ident.first.off << "g" << ident.first.gen << "b" << (int)ident.first.bw << ",";
    std::cout << "\n";
    for (const auto& ident : to_compare2)
        std::cout << "s" << ident.second.seg << "o" << ident.second.off << "g" << ident.second.gen << "b" << (int)ident.second.bw << ",";
    std::cout << "\n";
    std::cout << a_group << "\n";
    std::cout << b_group << "\n";
    // pc_b && foreach(a).(!pc_a || a!=b)
    // (sat iff not _b_ subseteq _a_)
    z3::context c;
    z3::solver s(c);

    z3::params p(c);
    p.set(":mbqi", true);
    if (!Config.is_set("--disabletimeout"))
        p.set(":timeout", 1000u);
    s.set(p);

    bool is_caching_enabled = Config.is_set("--enablecaching");
    Z3SubsetCall formula; // Structure for caching

    		// Try if the formula is in cache
    if (is_caching_enabled) {
        StopWatch s;
        s.start();

        std::copy(a_group.path_condition.begin(),
            a_group.path_condition.end(),
            std::back_inserter(formula.pc_a));
        std::copy(b_group.path_condition.begin(),
            b_group.path_condition.end(),
            std::back_inserter(formula.pc_b));

        for (const Definition &def : a_group.definitions)
            formula.pc_a.push_back(def.to_formula());

        for (const Definition &def : b_group.definitions)
            formula.pc_b.push_back(def.to_formula());

        std::copy(to_compare.begin(), to_compare.end(), std::back_inserter(formula.distinct));

        s.stop();

        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
            std::cout << "Building formula took " << s.getUs() << " us\n";

            	        // Test if this formula is in cache or not
        if (Z3cache.is_cached(formula)) {
            ++Statistics::getCounter(STAT_SMT_CACHED);
            return Z3cache.result() == z3::unsat;
        }
    }

    StopWatch solving_time;
    solving_time.start();

    z3::expr pc_a = c.bool_val(true);
    for (const auto &pc : a_group.path_condition)
        pc_a = pc_a && toz3(pc, 'a', c);
    z3::expr pc_b = c.bool_val(true);
    for (const auto &pc : b_group.path_condition)
        pc_b = pc_b && toz3(pc, 'b', c);

    for (const Definition &def : a_group.definitions)
        pc_a = pc_a && toz3(def.to_formula(), 'a', c);
    for (const Definition &def : b_group.definitions)
        pc_b = pc_b && toz3(def.to_formula(), 'b', c);

    z3::expr distinct = c.bool_val(false);

    for (const auto &vars : to_compare) {
        if (a_group.group.find(vars.first) == a_group.group.end())
            continue;
        z3::expr a_expr = toz3(Formula::buildIdentifier(vars.first), 'a', c);
        z3::expr b_expr = toz3(Formula::buildIdentifier(vars.second), 'b', c);

        distinct = distinct || (a_expr != b_expr);
    }

    std::vector< z3::expr > a_all_vars;

    for (const auto &var : a_group.collect_variables()) {
        a_all_vars.push_back(toz3(Formula::buildIdentifier(var), 'a', c));
    }

    z3::expr not_witness = !pc_a || distinct;
    s.add(pc_b);
    s.add(forall(a_all_vars, not_witness));

    ++Statistics::getCounter(STAT_SMT_CALLS);

    z3::check_result ret = s.check();
    if (ret == z3::unknown) {
        ++unknown_instances;
        if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
            if (Config.is_set("--vverbose"))
                std::cerr << "while checking:\n" << s;
            std::cerr << "\ngot 'unknown', reason: " << s.reason_unknown() << std::endl;
        }
    }

    if (ret == z3::sat)
        ++Statistics::getCounter(STAT_SUBSETEQ_SAT);
    else if (ret == z3::unsat)
        ++Statistics::getCounter(STAT_SUBSETEQ_UNSAT);
    else
        ++Statistics::getCounter(STAT_SUBSETEQ_UNKNOWN);

    solving_time.stop();

    if (is_caching_enabled)
        Z3cache.place(formula, ret, solving_time.getUs());
    
    // ;std::cout << "Model: " << s.get_model() << "\n";

    return ret == z3::unsat;
    // Test for syntax equality
    /*if (a_group.definitions == b_group.definitions) {
        bool equal_syntax = a_group.path_condition.size() == b_group.path_condition.size();
        for (size_t i = 0; equal_syntax && i != a_group.path_condition.size(); i++) {
            equal_syntax = a_group.path_condition[i]._rpn == b_group.path_condition[i]._rpn;
        }
        if (equal_syntax) {
            ++Statistics::getCounter(STAT_SUBSETEQ_SYNTAX_EQUAL);
            return true;
        }
    }
    
    // Check for equality: exists(b):(pc_b && foreach(a): (pc_a => a != b))
    // sat iff not b subseteq a
    
    z3::context c;
    z3::solver s(c);
    
    z3::params p(c);
    p.set(":mbqi", true);
    if (!Config.is_set("--disabletimeout")) {
        p.set(":timeout", 1000u);
    }
    s.set(p);
    
    static bool is_caching_enabled = Config.is_set("--enablecaching");
    static bool verbose = Config.is_set("--verbose") || Config.is_set("--vverbose");
    Z3SubsetCall formula;
    
    if (is_caching_enabled) {
        std::copy(a_group.path_condition.begin(), a_group.path_condition.end(),
            std::back_inserter(formula.pc_a));
        std::copy(b_group.path_condition.begin(), b_group.path_condition.end(),
            std::back_inserter(formula.pc_b));
        for (const Definition& def : a_group.definitions)
            formula.pc_a.push_back(def.to_formula());
        for (const Definition& def : b_group.definitions)
            formula.pc_b.push_back(def.to_formula());
        
	    if (Z3cache.is_cached(formula)) {
    	    ++Statistics::getCounter(STAT_SMT_CACHED);
            return Z3cache.result() == z3::unsat;
        }
    }
    
    StopWatch solving_time;
    solving_time.start();
    
    z3::expr pc_a = c.bool_val(true);
    for (const auto& pc : a_group.path_condition)
        pc_a = pc_a && toz3(pc, 'a', c);
    z3::expr pc_b = c.bool_val(true);
    for (const auto& pc : b_group.path_condition)
        pc_b = pc_b && toz3(pc, 'b', c);
    
    for (const auto& def : a_group.definitions)
        pc_a = pc_a && toz3(def.to_formula(), 'a', c);
    for (const auto& def : b_group.definitions)
        pc_b = pc_b && toz3(def.to_formula(), 'b', c);
    
    z3::expr distinct = c.bool_val(false);
    std::vector<Formula::Ident> ident_union;
    ident_union.reserve(a_group.group.size() + b_group.group.size());
    std::set_union(a_group.group.begin(), a_group.group.end(),
        b_group.group.begin(), b_group.group.end(), std::back_inserter(ident_union));
    for (const auto& ident : ident_union) {
        z3::expr a_expr = toz3(Formula::buildIdentifier(ident), 'a', c);
        z3::expr b_expr = toz3(Formula::buildIdentifier(ident), 'b', c);
        
        distinct = distinct || (a_expr != b_expr);
    }
    
    std::vector<z3::expr> a_all_vars;
    for (const auto& var : a_group.group) {
        a_all_vars.push_back(toz3(Formula::buildIdentifier(var), 'a', c));
    }
    
    z3::expr not_witness = !pc_a || distinct;
    s.add(pc_b);
    s.add(forall(a_all_vars, not_witness));
    
    Statistics::getCounter(STAT_SMT_CALLS);
    z3::check_result ret = s.check();
    
    if (ret == z3::unknown) {
        ++unknown_instances;
        if (verbose) {
            if (Config.is_set("--vverbose"))
                std::cerr << "while checking:\n" << s;
            std::cerr << "\ngot 'unknown', reason: " << s.reason_unknown() << std::endl;
        }
    }
    
    if (ret == z3::sat)
        ++Statistics::getCounter(STAT_SUBSETEQ_SAT);
    else if (ret == z3::unsat)
        ++Statistics::getCounter(STAT_SUBSETEQ_UNSAT);
    else
        ++Statistics::getCounter(STAT_SUBSETEQ_UNKNOWN);
    
    solving_time.stop();
    
    if (is_caching_enabled)
        Z3cache.place(formula, ret, solving_time.getUs());
        
    return ret == z3::unsat;*/
}

bool SMTStorePartial::subseteq(const SMTStorePartial &a, const SMTStorePartial &b) // There were mismatched letters!
{
    using dependency_group_r = std::reference_wrapper<const dependency_group>;
    
    std::vector<dependency_group_r> a_group;
    std::vector<dependency_group_r> b_group;
    
    for (const auto& value : a.sym_data)
        a_group.emplace_back(value.second);
    for (const auto& value : b.sym_data)
        b_group.emplace_back(value.second);
    
    std::cout << "State A:\n" << a << "\nState B:\n" << b << "\n";
    
    std::cout << "Subset A_____________________________________________\n";
    bool subset1 = subseteq(a_group, b_group, a, b);
    if (subset1)
        std::cout << "Subset\n";
    else
        std::cout << "No subset\n";
    
    std::vector<std::pair<dependency_group_r, dependency_group_r>> same;
    std::vector<dependency_group_r> unmatched_a, unmatched_b;
    size_t i = 0;
    size_t j = 0;
    while (i != a_group.size() || j != b_group.size()) {
        if (i == a_group.size()) {
            unmatched_b.emplace_back(b_group[j]);
            j++;
            continue;
        }
        if (j == b_group.size()) {
            unmatched_a.emplace_back(a_group[i]);
            i++;
            continue;
        }
        
        if (a_group[i].get() == b_group[j].get()) {
            same.emplace_back(a_group[i], b_group[j]);
            i++; j++;
        }
        else if (a_group[i].get() < b_group[j].get()) {
            unmatched_a.emplace_back(a_group[i]);
            i++;
        }
        else {
            unmatched_b.emplace_back(b_group[j]);
            j++;
        }
    }
    /*for (size_t i = 0; i != a_group.size(); i++) {
        size_t j = i;
        for (; j != b_group.size(); j++) {
            if (a_group[i].get() == b_group[j].get()) {
                same.emplace_back(a_group[i], b_group[i]);
                j = b_group.size() + 1;
                break;
            }
            else if (i == j) {
                unmatched_b.emplace_back(b_group[j]);
            }
        }   
        if (j == b_group.size())
            unmatched_a.emplace_back(a_group[i]);
    }*/
    
    std::cout << "Subset B__________________________________________\n";
    
    bool result = true;
    for (auto& pair : same) {
        if (!subseteq(std::vector<dependency_group_r>({pair.first}),
                std::vector<dependency_group_r>({pair.second}), a, b))
        {
            result = false;
            assert(result == subset1);
            break;
        }
    }
    
    
    result = result && subseteq(unmatched_a, unmatched_b, a, b);
    
    assert(subset1 == result);
    return result;
}

}

