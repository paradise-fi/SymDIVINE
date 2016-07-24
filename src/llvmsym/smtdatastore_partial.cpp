#include <algorithm>
#include <functional>
#include <llvmsym/smtdatastore_partial.h>
#include <toolkit/z3cache.h>
#include <toolkit/utils.h>

namespace llvm_sym {

unsigned SMTStorePartial::unknown_instances = 0;
    
std::ostream& operator<<(std::ostream& o, const SMTStorePartial::dependency_group& g) {
    o << "Variables: ";
    for (const auto& ident : g.get_group())
        o << "seg" << ident.seg << "_off" << ident.off << "_gen" << ident.gen << ", ";
    o << "; Path condition: ";
    for (const auto& pc : g.get_path_condition())
        o << pc << ", ";
    o << "; Definitions: ";
    for (const auto& def : g.get_definitions())
        o << def.to_formula() << ", ";
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

bool SMTStorePartial::empty()
{
    z3::context c;
	z3::solver solver(c);
	ExprSimplifier simp(c, true);

	bool smtstore_res = false;
	if (test_run)
		smtstore_res = store.empty();

    std::vector<dependency_group*> set;
	static bool simplify = Config.is_set("--q3bsimplify");
	z3::expr query = c.bool_val(true);
    for (auto& group : sym_data) {
        TriState s = group.second.get_state();
        if (s == TriState::TRUE)
            continue;
	    if (s == TriState::FALSE) {
		    assert(!test_run || smtstore_res);
		    return true;
	    }
        for (const Definition &def : group.second.get_definitions())
            query = query && toz3(def.to_formula(), 'a', c);

        for (const Formula &pc : group.second.get_path_condition())
            query = query && toz3(pc, 'a', c); 
        set.push_back(&group.second);
    }

    if (simplify) {
	    query = simp.Simplify(query);
    }
    
    z3::check_result r = solve_query_qf(solver, query);
    assert(r != z3::unknown);
    
    if (r == z3::unsat) {
        for (auto& g : set)
            g->set_state(TriState::TRUE);
	    assert(!test_run || smtstore_res);
        return true;
    }
    
	assert(!test_run || !smtstore_res);
    return false;
}
    
bool SMTStorePartial::syntax_equal(const dependency_group a,
        const dependency_group b)
{
    if (a.get_definitions() == b.get_definitions()) {
        bool equal_syntax = a.get_path_condition().size() == b.get_path_condition().size();
        for (size_t i = 0; equal_syntax && i < a.get_path_condition().size(); ++i) {
            if (a.get_path_condition()[i]._rpn != b.get_path_condition()[i]._rpn)
                equal_syntax = false;
        }
        if (equal_syntax) 
            return true;
    } 
    
    return false;
}

bool SMTStorePartial::subseteq(
        const std::vector<std::reference_wrapper<const dependency_group>>& a_g,
        const std::vector<std::reference_wrapper<const dependency_group>>& b_g,
        const std::map<Formula::Ident, Formula::Ident>& to_compare, bool timeout,
        bool is_caching_enabled)
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
    
    ++Statistics::getCounter(SUBSETEQ_CALLS);
    if (syntax_equal(a_group, b_group)) {
        ++Statistics::getCounter(SUBSETEQ_SYNTAX_EQUAL);
        return true;
    }
    
    // pc_b && foreach(a).(!pc_a || a!=b)
    // (sat iff not _b_ subseteq _a_)
    static z3::context c;
    z3::solver s(c);
    ExprSimplifier simp(c, true);
    static bool simplify = Config.is_set("--q3bsimplify");

    z3::params p(c);
    p.set(":mbqi", true);
    if (timeout)
        p.set(":timeout", 1000u);
    s.set(p);

    Z3SubsetCall formula; // Structure for caching

    // Try if the formula is in cache
    if (is_caching_enabled) {
        StopWatch s;
        s.start();

        std::copy(a_group.get_path_condition().begin(),
            a_group.get_path_condition().end(),
            std::back_inserter(formula.pc_a));
        std::copy(b_group.get_path_condition().begin(),
            b_group.get_path_condition().end(),
            std::back_inserter(formula.pc_b));

        for (const Definition &def : a_group.get_definitions())
            formula.pc_a.push_back(def.to_formula());

        for (const Definition &def : b_group.get_definitions())
            formula.pc_b.push_back(def.to_formula());

        std::copy(to_compare.begin(), to_compare.end(), std::back_inserter(formula.distinct));

        s.stop();

        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
            std::cout << "Building formula took " << s.getUs() << " us\n";

        // Test if the formula is in cache or not
        if (Z3cache.is_cached(formula)) {
            ++Statistics::getCounter(SMT_CACHED);
            return Z3cache.result() == z3::unsat;
        }
    }

    StopWatch solving_time;
    solving_time.start();

    z3::expr pc_a = c.bool_val(true);
    for (const auto &pc : a_group.get_path_condition())
        pc_a = pc_a && toz3(pc, 'a', c);
    z3::expr pc_b = c.bool_val(true);
    for (const auto &pc : b_group.get_path_condition())
        pc_b = pc_b && toz3(pc, 'b', c);

    for (const Definition &def : a_group.get_definitions())
        pc_a = pc_a && toz3(def.to_formula(), 'a', c);
    for (const Definition &def : b_group.get_definitions())
        pc_b = pc_b && toz3(def.to_formula(), 'b', c);

    z3::expr distinct = c.bool_val(false);

    for (const auto &vars : to_compare) {
        z3::expr a_expr = toz3(Formula::buildIdentifier(vars.first), 'a', c);
        z3::expr b_expr = toz3(Formula::buildIdentifier(vars.second), 'b', c);

        distinct = distinct || (a_expr != b_expr);
    }

    std::vector< z3::expr > a_all_vars;
    for (const auto &var : a_group.collect_variables()) {
        a_all_vars.push_back(toz3(Formula::buildIdentifier(var), 'a', c));
    }    

    z3::expr not_witness = !pc_a || distinct;
	z3::expr query = pc_b;
    if (!a_all_vars.empty())
        query = query && forall(a_all_vars, not_witness);

    if (simplify) {
        query = simp.Simplify(query);
    }
    
    z3::check_result ret = solve_query_q(s, query);
    if (ret == z3::unknown) {
        ++unknown_instances;
        if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
            if (Config.is_set("--vverbose"))
                std::cerr << "while checking:\n" << s;
            std::cerr << "\ngot 'unknown', reason: " << s.reason_unknown() << std::endl;
        }
    }

    solving_time.stop();

    if (is_caching_enabled)
        Z3cache.place(formula, ret, solving_time.getUs());

    return ret == z3::unsat;
}

bool SMTStorePartial::subseteq(const SMTStorePartial &a, const SMTStorePartial &b,
        bool timeout, bool caching) 
{
    using dependency_group_r = std::reference_wrapper<const dependency_group>;
    std::vector<dependency_group_r> a_group;
    std::vector<dependency_group_r> b_group;
    
    for (const auto& value : a.sym_data)
        a_group.emplace_back(value.second);
    for (const auto& value : b.sym_data)
        b_group.emplace_back(value.second);
    
    std::map<Formula::Ident, Formula::Ident> a_to_b;
    for (unsigned s = 0; s < a.generations.size(); ++s) {
        assert(a.generations[s].size() == b.generations[s].size());
        for (unsigned offset = 0; offset < a.generations[s].size(); ++offset) {
            Value var;
            var.type = Value::Type::Variable;
            var.variable.segmentId = s;
            var.variable.offset = offset;

            Formula::Ident a_atom = a.build_item(var);
            Formula::Ident b_atom = b.build_item(var);

            if (!a.depends_on(var) && !b.depends_on(var))
                continue;

            a_to_b.insert({ a_atom, b_atom });
        }
    }
    
    bool full_check_result;
    if (Config.is_set("--testvalidity"))
        full_check_result = subseteq(a_group, b_group, a_to_b, timeout, caching);
    
    using DepSet = UnionSet<
        const dependency_group*, std::map<Formula::Ident, Formula::Ident>>;
    
    std::map<Formula::Ident, DepSet*> a_id_info;
    std::vector<DepSet> a_sets;
    a_sets.reserve(a_group.size());
    for (const auto& group : a_group) {
        a_sets.emplace_back(&group.get());
        for (const auto& id : group.get().get_group()) {
            a_id_info.insert({id, &a_sets.back()});
        }
    }

    std::map<Formula::Ident, DepSet*> b_id_info;
    std::vector<DepSet> b_sets;
    b_sets.reserve(b_group.size());
    for (const auto& group : b_group) {
        b_sets.emplace_back(&group.get());
        for (const auto& id : group.get().get_group()) {
            b_id_info.insert({id, &b_sets.back()});
        }
    }
    
    std::vector<std::pair<
        std::vector<dependency_group_r>, std::vector<dependency_group_r>>> compare_groups;
    
    for (const auto& id_pair : a_to_b) {
        assert(a_id_info.find(id_pair.first.no_gen()) != a_id_info.end());
        assert(b_id_info.find(id_pair.second.no_gen()) != b_id_info.end());
        
        DepSet* a_set = a_id_info.find(id_pair.first.no_gen())->second;
        DepSet* b_set = b_id_info.find(id_pair.second.no_gen())->second;
        
        a_set->get_set()->tag.insert(id_pair);
        
        if (a_set->get_set() != b_set->get_set()) {
            join(a_set->get_set(), b_set->get_set(),
                [](std::map<Formula::Ident, Formula::Ident> a,
                   std::map<Formula::Ident, Formula::Ident> b)
                {
                    a.insert(b.begin(), b.end());
                    assert(!a.empty());
                    return a;
                });
        }
        assert(!a_set->get_set()->tag.empty());
        assert(!b_set->get_set()->tag.empty());
    }
    
    std::map<const dependency_group*, std::tuple<
        std::vector<dependency_group_r>,
        std::vector<dependency_group_r>,
        std::map<Formula::Ident, Formula::Ident>>> compares;
    
    for (DepSet& g : a_sets) {
        auto& tup = compares[g.get_set()->data];
        std::get<0>(tup).push_back(*g.data);
        if (g.is_root()) {
            std::get<2>(tup) = g.tag;
        }
    }
    
    for (DepSet& g : b_sets) {
        auto& tup = compares[g.get_set()->data];
        std::get<1>(tup).push_back(*g.data);
        if (g.is_root()) {
            std::get<2>(tup) = g.tag;
        }
    }
    
    bool result = true;
    for (const auto& pair : compares) {
        if (std::get<2>(pair.second).empty())
            continue;
        if (!subseteq(std::get<0>(pair.second), std::get<1>(pair.second), std::get<2>(pair.second), timeout, caching))
        {
            result = false;
            if (Config.is_set("--testvalidity")) {
                if (result != full_check_result) {
                    std::cout << "Different result other than join of the groups was obtained!\n";
                    abort();
                }
            }
            break;
        }
    }

    if (Config.is_set("--testvalidity")) {
        bool fail = false;
        if (result != full_check_result) {
            std::cout << "Different result other than join of the groups was obtained!\n";
            fail = true;
        }
        if (result != SMTStore::subseteq(a.store, b.store, false, false)) {
            std::cout << "Different result other than SMTStore was obtained!\n";
            fail = true;
        }
        
        if (fail) {
            std::cout << "Comapres: \n";
            for (const auto& item : compares) {
                for (const auto& p : std::get<0>(item.second))
                    std::cout << p << ", ";
                std::cout << " | ";
                for (const auto& p : std::get<1>(item.second))
                    std::cout << p << ", ";
        
                std::cout << "\n\n";
            }
            abort();
        }
    }
    
    return result;
}

}

