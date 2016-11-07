#include <algorithm>
#include <llvmsym/smtdatastore.h>
#include <toolkit/z3cache.h>

namespace llvm_sym {

    unsigned SMTStore::unknown_instances = 0;

    std::ostream & operator<<(std::ostream & o, const SMTStore &v) {
        o << "data:\n";

        o << "\npath condition:\n";
        for (const Formula &pc : v.path_condition)
            o << pc << '\n';

        o << "\n" << v.definitions.size() << " definitions:\n";
        for (const Definition &def : v.definitions)
            o << def.to_formula() << "\n\n";

        return o;
    }

    bool SMTStore::empty() {
        try {
            static bool simplify = Config.is_set("--q3bsimplify");
            if (path_condition.size() == 0)
                return false;

            z3::context c;
            z3::expr pc = c.bool_val(true);

            z3::solver s(c);

            for (const Definition &def : definitions)
                pc = pc && toz3(def.to_formula(), 'a', c);

            for (const Formula &p : path_condition)
                pc = pc && toz3(p, 'a', c);

            if (simplify) {
                ExprSimplifier simp(c, true);
                pc = simp.Simplify(pc);
            }

            z3::check_result ret = solve_query_qf(s, pc);

            assert(ret != z3::unknown);

            return ret == z3::unsat;
        }
        catch (const z3::exception& e) {
            std::cerr << "Cannot perform empty operation: " << e.msg() << "\n";
            throw e;
        }
    }

    bool SMTStore::subseteq(const SMTStore &b, const SMTStore &a, bool timeout,
        bool is_caching_enabled)
    {
        static bool simplify = Config.is_set("--q3bsimplify");
        ++Statistics::getCounter(SUBSETEQ_CALLS);
        if (a.definitions == b.definitions) {
            bool equal_syntax = a.path_condition.size() == b.path_condition.size();
            for (size_t i = 0; equal_syntax && i < a.path_condition.size(); ++i) {
                if (a.path_condition[i]._rpn != b.path_condition[i]._rpn)
                    equal_syntax = false;
            }
            if (equal_syntax) {
                ++Statistics::getCounter(SUBSETEQ_SYNTAX_EQUAL);
                return true;
            }
        }

        std::map< Formula::Ident, Formula::Ident > to_compare;
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

                to_compare.insert(std::make_pair(a_atom, b_atom));
            }
        }

        if (to_compare.empty())
            return true;

        // pc_b && foreach(a).(!pc_a || a!=b)
        // (sat iff not _b_ subseteq _a_)
        z3::context c;
        z3::solver s(c);

        z3::params p(c);
        p.set(":mbqi", true);
        if (timeout)
            p.set(":timeout", 1000u);
        s.set(p);

        Z3SubsetCall formula; // Structure for caching
        bool cached_result = false;
        bool retrieved_from_cache = false;

        // Try if the formula is in cache
        if (is_caching_enabled) {
            StopWatch s;
            s.start();

            std::copy(a.path_condition.begin(), a.path_condition.end(),
                std::back_inserter(formula.pc_a));
            std::copy(b.path_condition.begin(), b.path_condition.end(),
                std::back_inserter(formula.pc_b));

            for (const Definition &def : a.definitions)
                formula.pc_a.push_back(def.to_formula());

            for (const Definition &def : b.definitions)
                formula.pc_b.push_back(def.to_formula());

            std::copy(to_compare.begin(), to_compare.end(), std::back_inserter(formula.distinct));

            s.stop();

            if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
                std::cout << "Building formula took " << s.getUs() << " us\n";

            // Test if this formula is in cache or not
            if (Z3cache.is_cached(formula)) {
                ++Statistics::getCounter(SMT_CACHED);
                cached_result = Z3cache.result() == z3::unsat;
                retrieved_from_cache = true;
                if (!Config.is_set("--testvalidity"))
                    return cached_result;
            }
        }

        StopWatch solving_time;
        solving_time.start();

        z3::expr pc_a = c.bool_val(true);
        for (const auto &pc : a.path_condition)
            pc_a = pc_a && toz3(pc, 'a', c);
        z3::expr pc_b = c.bool_val(true);
        for (const auto &pc : b.path_condition)
            pc_b = pc_b && toz3(pc, 'b', c);

        for (const Definition &def : a.definitions)
            pc_a = pc_a && toz3(def.to_formula(), 'a', c);
        for (const Definition &def : b.definitions)
            pc_b = pc_b && toz3(def.to_formula(), 'b', c);

        z3::expr distinct = c.bool_val(false);

        for (const auto &vars : to_compare) {
            z3::expr a_expr = toz3(Formula::buildIdentifier(vars.first), 'a', c);
            z3::expr b_expr = toz3(Formula::buildIdentifier(vars.second), 'b', c);

            distinct = distinct || (a_expr != b_expr);
        }

        std::vector< z3::expr > a_all_vars;

        for (const auto &var : a.collect_variables()) {
            a_all_vars.push_back(toz3(Formula::buildIdentifier(var), 'a', c));
        }

        z3::expr not_witness = !pc_a || distinct;
        z3::expr query = pc_b && forall(a_all_vars, not_witness);

        if (simplify) {
            ExprSimplifier simp(c, true);
            query = simp.Simplify(query);
        }

        z3::check_result ret = solve_query_q(s, query);

        if (ret == z3::unknown) {
            ++unknown_instances;
            ++Statistics::getCounter(SOLVER_UNKNOWN);
            if (Config.is_set("--verbose") || Config.is_set("--vverbose")) {
                if (Config.is_set("--vverbose"))
                    std::cerr << "while checking:\n" << s;
                std::cerr << "\ngot 'unknown', reason: " << s.reason_unknown() << std::endl;
            }
        }

        //FormulaeCapture::insert(s.to_smt2(), ret);

        solving_time.stop();

        if (is_caching_enabled)
            Z3cache.place(formula, ret, solving_time.getUs());

        bool real_result = ret == z3::unsat;

        if (is_caching_enabled && Config.is_set("--testvalidity") &&
            retrieved_from_cache && real_result != cached_result)
        {
            std::cout << "Got different result from cache!\n";
            abort();
        }

        return real_result;
    }

}

