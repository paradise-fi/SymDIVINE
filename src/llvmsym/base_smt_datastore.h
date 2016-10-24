#pragma once

#include <llvmsym/datastore.h>
#include <llvmsym/formula/rpn.h>
#include <llvmsym/formula/z3.h>
#include <llvmsym/programutils/statistics.h>
#include <llvmsym/programutils/config.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <toolkit/utils.h>

#define SUBSETEQ_CALLS "Subseteq queries"
#define SUBSETEQ_SYNTAX_EQUAL "Subseteq on syntax"
#define SMT_CACHED "Equal query cached"
#define QF_SIMP "QF queries solved via simplification"
#define QF_N_SIMP "QF queries solved via solver"
#define Q_SIMP "Q queries solved via simplification"
#define Q_N_SIMP "Q queries solved via solver"
#define SOLVER_UNKNOWN "Solver unknown"

namespace llvm_sym {

    template<class StoreType>
	class BaseSMTStore : public DataStore {
	protected:
		virtual Formula::Ident build_item(Value val) const = 0;
		virtual Formula build_expression(Value val) const = 0;
		virtual Formula build_expression(Value val, bool advance_generation) = 0;
		virtual int get_generation(unsigned segId, unsigned offset) const = 0;
		virtual int get_generation(unsigned segId, unsigned offset, bool advance_generation) = 0;
		virtual int get_generation(Value val, bool advance_generation = false) = 0;
		virtual void push_condition(const Formula &f) = 0;
    	virtual void push_definition(Value symbol_id, const Formula &def) = 0;

	public:
		virtual void implement_add(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr + b_expr);
		}

		virtual void implement_mult(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr * b_expr);
		}

		virtual void implement_sub(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr - b_expr);
		}

		virtual void implement_div(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr / b_expr);
		}

		virtual void implement_urem(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr.buildURem(b_expr));
		}

		virtual void implement_srem(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr.buildSRem(b_expr));
		}

		virtual void implement_and(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr & b_expr);
		}

		virtual void implement_or(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr | b_expr);
		}

		virtual void implement_xor(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr ^ b_expr);
		}

		virtual void implement_left_shift(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr << b_expr);
		}

		virtual void implement_right_shift(Value result_id, Value a_id, Value b_id) final {
			Formula a_expr = build_expression(a_id);
			Formula b_expr = build_expression(b_id);
			push_definition(result_id, a_expr >> b_expr);
		}

		virtual void implement_store(Value result_id, Value what) final {
			Formula what_expr = build_expression(what);
			push_definition(result_id, what_expr);
		}

		virtual void implement_ZExt(Value result_id, Value a_id, int bw) final {
			Formula what_expr = build_expression(a_id);
			push_definition(result_id, what_expr.buildZExt(bw));
		}

		virtual void implement_SExt(Value result_id, Value a_id, int bw) final {
			Formula what_expr = build_expression(a_id);
			push_definition(result_id, what_expr.buildSExt(bw));
		}

		virtual void implement_Trunc(Value result_id, Value a_id, int bw) final {
			Formula what_expr = build_expression(a_id);
			push_definition(result_id, what_expr.buildTrunc(bw - 1, 0));
		}

		virtual void implement_inttoptr(Value result_id, Value a_id) final {
			Formula what_expr = build_expression(a_id);
			push_definition(result_id, what_expr);
		}

		virtual void implement_ptrtoint(Value result_id, Value a_id) final {
			Formula what_expr = build_expression(a_id);
			push_definition(result_id, what_expr);
		}

		virtual void prune(Value a, Value b, ICmp_Op op) final {
			Formula a_expr = build_expression(a);
			Formula b_expr = build_expression(b);
			switch (op) {
			case ICmp_Op::EQ:
				push_condition(a_expr == b_expr);
				break;
			case ICmp_Op::NE:
				push_condition(a_expr != b_expr);
				break;
			case ICmp_Op::UGT:
				push_condition(a_expr.buildUGT(b_expr));
				break;
			case ICmp_Op::SGT:
				push_condition(a_expr > b_expr);
				break;
			case ICmp_Op::UGE:
				push_condition(a_expr.buildUGEq(b_expr));
				break;
			case ICmp_Op::SGE:
				push_condition(a_expr >= b_expr);
				break;
			case ICmp_Op::ULT:
				push_condition(a_expr.buildULT(b_expr));
				break;
			case ICmp_Op::SLT:
				push_condition(a_expr < b_expr);
				break;
			case ICmp_Op::ULE:
				push_condition(a_expr.buildULEq(b_expr));
				break;
			case ICmp_Op::SLE:
				push_condition(a_expr <= b_expr);
				break;
			}
		}

		virtual void implement_input(Value input_variable, unsigned bw) final {
			get_generation(input_variable, true);
		}

		bool equal(const StoreType& snd) {
			return subseteq(*this, snd) && subseteq(snd, *this);
		}

        template <bool quantified>
		static z3::check_result solve_query(z3::solver& s, z3::expr& e) {
			const char* simp = quantified ? Q_SIMP : QF_SIMP;
			const char* solv = quantified ? Q_N_SIMP : QF_N_SIMP;
            switch(is_const(e))
            {
            case TriState::TRUE:
	            ++Statistics::getCounter(simp);
	            return z3::sat;
            case TriState::FALSE:
	            ++Statistics::getCounter(simp);
	            return z3::unsat;
            case TriState::UNKNOWN:
	            ++Statistics::getCounter(solv);
	            s.push();
	            s.add(e);
	            auto res = s.check();
	            s.pop();
	            return res;
            }
        }

		static z3::check_result solve_query_qf(z3::solver& s, z3::expr& e) {
			return solve_query<false>(s, e);
        }

		static z3::check_result solve_query_q(z3::solver& s, z3::expr& e) {
			return solve_query<true>(s, e);
		}
	};
}
