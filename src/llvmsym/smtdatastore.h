#pragma once

#include <llvmsym/base_smt_datastore.h>
#include <llvmsym/formula/rpn.h>
#include <llvmsym/formula/z3.h>
#include <llvmsym/programutils/statistics.h>
#include <llvmsym/programutils/config.h>
#include <vector>

#define STAT_SUBSETEQ_CALLS "SMT calls Subseteq()"
#define STAT_EMPTY_CALLS "SMT calls Empty()"
#define STAT_SMT_CALLS "SMT queries"
#define STAT_SUBSETEQ_SAT "SMT queries: SAT"
#define STAT_SUBSETEQ_UNSAT "SMT queries: unSAT"
#define STAT_SUBSETEQ_UNKNOWN "SMT queries: unSAT"
#define STAT_SUBSETEQ_SYNTAX_EQUAL "SMT subseteq on syntax. equal"
#define STAT_SMT_SIMPLIFY_CALLS "SMT simplify calls"

namespace llvm_sym {

	class SMTStore : public BaseSMTStore<SMTStore> {
		std::vector< short unsigned > segments_mapping;
		std::vector< std::vector< short unsigned > > generations;
		std::vector< std::vector< char > > bitWidths;

		std::vector< Formula > path_condition;
		std::vector< Definition > definitions;
		int fst_unused_id = 0;
		static unsigned unknown_instances;

		Formula::Ident build_item(Value val) const {
			assert(val.type == Value::Type::Variable);
			int segment_mapped_to = segments_mapping[val.variable.segmentId];
			return Formula::Ident(
			            segment_mapped_to,
			            val.variable.offset,
			            get_generation(val.variable.segmentId, val.variable.offset),
			            bitWidths[val.variable.segmentId][val.variable.offset]
			            );
		}

		Formula build_expression(Value val) const {
			if (val.type == Value::Type::Constant)
				return Formula::buildConstant(val.constant.value, val.constant.bw);
			else {
				int segment_mapped_to = segments_mapping[val.variable.segmentId];
				return Formula::buildIdentifier(Formula::Ident(
				            segment_mapped_to,
				            val.variable.offset,
				            get_generation(val.variable.segmentId, val.variable.offset),
				            bitWidths[val.variable.segmentId][val.variable.offset]
				            ));
			}
		}

		Formula build_expression(Value val, bool advance_generation) {
			if (val.type == Value::Type::Constant)
				return Formula::buildConstant(val.constant.value, val.constant.bw);
			else {
				int segment_mapped_to = segments_mapping[val.variable.segmentId];
				return Formula::buildIdentifier(Formula::Ident(
				            segment_mapped_to,
				            val.variable.offset,
				            get_generation(val.variable.segmentId, val.variable.offset, advance_generation),
				            bitWidths[val.variable.segmentId][val.variable.offset]
				            ));
			}
		}

		int get_generation(unsigned segId, unsigned offset) const {
			assert(segId < generations.size());
			assert(offset < generations[segId].size());
			return generations[segId][offset];
		}

		int get_generation(unsigned segId, unsigned offset, bool advance_generation) {
			assert(segId < generations.size());
			assert(offset < generations[segId].size());
			short unsigned &g = generations[segId][offset];
			const int seg_m = segments_mapping[segId];
			if (advance_generation) {
				const auto &is_variable_predicate = [offset, g, seg_m](const Definition &d) {
					return d.isInSegment(seg_m) && d.isOffset(offset) && d.isGeneration(g);
				};
				removeDefinitions(is_variable_predicate);
				++g;
			}

			return g;
		}
		int get_generation(Value val, bool advance_generation = false) {
			assert(val.type == Value::Type::Variable);
			return get_generation(val.variable.segmentId, val.variable.offset, advance_generation);
		}	

		void push_condition(const Formula &f) {
			path_condition.push_back(f);
			simplify();
		}

		void push_definition(Value symbol_id, const Formula &def) {
			assert(def.sane());
			assert(!def._rpn.empty());
			int segment_mapped_to = segments_mapping[symbol_id.variable.segmentId];
			auto ident = Formula::Ident(
			                segment_mapped_to,
			                symbol_id.variable.offset,
			                get_generation(symbol_id.variable.segmentId, symbol_id.variable.offset, true),
			                bitWidths[symbol_id.variable.segmentId][symbol_id.variable.offset]
			);
			const Definition whole_def = Definition(ident, def);
    		// TEMP
    		try {
        		z3::context c;
        		toz3(whole_def.to_formula(), 'a', c);
    		}
    		catch (const z3::exception& e) {
        		throw e;
    		}
    		// TEMP
			auto it = std::upper_bound(definitions.begin(), definitions.end(), whole_def);
			definitions.insert(it, whole_def);
		}

		std::vector< Formula::Ident > collectVariables() const {
			std::vector< Formula::Ident > ret;

			for (const auto &pc : path_condition)
				pc.collectVariables(ret);
			for (const auto &def : definitions)
				def.to_formula().collectVariables(ret);

			return ret;
		}

		bool dependsOn(Value symbol_id) const {
			int segment_mapped_to = segments_mapping[symbol_id.variable.segmentId];
			int offset = symbol_id.variable.offset;

			int gen = get_generation(symbol_id.variable.segmentId, offset);

			return dependsOn(segment_mapped_to, offset, gen);
		}

		bool dependsOn(int seg, int offset, int generation) const {
			bool defs_depends = false;
			for (auto &def : definitions)
				defs_depends = defs_depends
				    || def.dependsOn(seg, offset, generation);

			if (defs_depends)
				return true;

			bool pc_depends = false;
			for (auto &pc : path_condition)
				pc_depends = pc_depends
				    || pc.dependsOn(seg, offset, generation);

			return pc_depends;
		}

	public:

		void simplify() {
			++Statistics::getCounter(STAT_SMT_SIMPLIFY_CALLS);
			if (path_condition.empty())
				return;
			/*if (unknown_instances <= 4)
			    // instances are very easy (solving time < 10ms), it is a waste to simplify
				return;*/

			Formula conj;
			for (Formula &pc : path_condition)
				conj = conj && pc;

			if (Config.is_set("--cheapsimplify")) {
				auto simplified = cheap_simplify(conj);
				path_condition.resize(1);
				path_condition.back() = simplified;
			}
			else if (!Config.is_set("--dontsimplify")) {
			    // regular, full, expensive simplify
				auto simplified = llvm_sym::simplify(conj);
				path_condition.resize(1);
				path_condition.back() = simplified;
			}
		}

		virtual size_t getSize() const {
			int size = representation_size(segments_mapping, generations, bitWidths, fst_unused_id);
			size += sizeof(size_t);
			for (const Definition &f : definitions) {
				size += representation_size(f.symbol);
				size += representation_size(f.def._rpn);
			}
			size += sizeof(size_t);
			for (const Formula &pc : path_condition) {
				size += representation_size(pc._rpn);
			}

			return size;
		}

		virtual void writeData(char * &mem) const {
			blobWrite(mem, segments_mapping, generations, bitWidths, fst_unused_id);

			blobWrite(mem, definitions.size());
			for (const Definition &f : definitions) {
				blobWrite(mem, f.symbol);
				blobWrite(mem, f.def._rpn);
			}
			blobWrite(mem, path_condition.size());
			for (const Formula &pc : path_condition) {
				blobWrite(mem, pc._rpn);
			}
		}
    
		virtual void readData(const char * &mem) {
			blobRead(mem, segments_mapping, generations, bitWidths, fst_unused_id);

			size_t definitions_size;
			blobRead(mem, definitions_size);
			definitions.resize(definitions_size);
			for (unsigned i = 0; i < definitions_size; ++i) {
				blobRead(mem, definitions[i].symbol);
				blobRead(mem, definitions[i].def._rpn);
			}
			size_t pc_size;
			blobRead(mem, pc_size);
			path_condition.resize(pc_size);
			for (unsigned i = 0; i < pc_size; ++i) {
				blobRead(mem, path_condition[i]._rpn);
			}

			assert(segments_mapping.size() == generations.size());
			assert(segments_mapping.size() == bitWidths.size());
		}

		void pushPropGuard(const Formula &g) {
			Formula f = g;
			std::vector<Formula::Ident> ids;
			for (auto &i : f._rpn) {
				if (i.kind == Formula::Item::Kind::Identifier) {
					i.id.gen = get_generation(i.id.seg, i.id.off, false);
					i.id.bw = bitWidths[i.id.seg][i.id.off];
				}
			}
			push_condition(f);
		}
    	
    	int getBitWidth(Value val) {
        	return bitWidths[val.variable.segmentId][val.variable.offset];
    	}

		virtual void addSegment(unsigned id, const std::vector< int > &bit_widths) {
			segments_mapping.insert(segments_mapping.begin() + id, fst_unused_id++);
			generations.emplace(generations.begin() + id, bit_widths.size(), 0);
			bitWidths.insert(bitWidths.begin() + id, std::vector< char >(bit_widths.size()));

			for (size_t i = 0; i < bit_widths.size(); ++i) {
				bitWidths[id][i] = static_cast< char >(bit_widths[i]);
			}

			for (unsigned offset = 0; offset < bit_widths.size(); ++offset) {
				Value new_val;
				new_val.type = Value::Type::Variable;
				new_val.variable.segmentId = id;
				new_val.variable.offset = offset;
				//implement_store( new_val, Value( 0, bit_widths[ offset ] ) );
			}
		}

		virtual void eraseSegment(int id) {
			int mapped_to = segments_mapping[id];
			const auto &in_segment_predicate = [mapped_to](const Definition &d) {
				return d.isInSegment(mapped_to);
			};
			removeDefinitions(in_segment_predicate);
			//removeConditions( in_segment_predicate_f );
			segments_mapping.erase(segments_mapping.begin() + id);
			generations.erase(generations.begin() + id);
			bitWidths.erase(bitWidths.begin() + id);
			simplify();
		}

		template < typename Predicate >
			void removeDefinitions(Predicate pred) {
				std::vector< Definition > to_remove(definitions.size());
				auto it = std::copy_if(definitions.begin(), definitions.end(), to_remove.begin(), pred);
				to_remove.resize(it - to_remove.begin());

				it = std::remove_if(definitions.begin(), definitions.end(), pred);
				definitions.resize(it - definitions.begin());

				bool change;
				do {
					change = false;
					for (Formula &pc : path_condition) {
						for (const Definition &def : to_remove) {
							Formula new_pc = pc.substitute(def.getIdent(), def.getDef());
							if (new_pc._rpn != pc._rpn) {
								change = true;
								pc = new_pc;
							}
						}
					}

					for (Definition &def : definitions) {
						for (auto def_to_remove = to_remove.crbegin(); def_to_remove != to_remove.crend(); ++def_to_remove) {
							Definition new_def = def.substitute(def_to_remove->getIdent(), def_to_remove->getDef());
							if (new_def.def._rpn != def.def._rpn) {
								def = def.substitute(def_to_remove->getIdent(), def_to_remove->getDef());
								change = true;
							}
						}
					}
				} while (change);
				std::sort(definitions.begin(), definitions.end());
			}

		template < typename Predicate >
			void removeConditions(Predicate pred) {
			    // precondition: run removeDefinition( pred ) to be sure that the symbols
			    // are not used anywhere else
				auto it = std::remove_if(path_condition.begin(), path_condition.end(), pred);
				path_condition.resize(it - path_condition.begin());
			}

		bool equal(const SMTStore &snd) {
			assert(segments_mapping.size() == snd.segments_mapping.size());

			return subseteq(*this, snd) && subseteq(snd, *this);
		}

		virtual bool empty() const;

		static bool subseteq(const SMTStore &a, const SMTStore &b);

		void clear() {
			path_condition.clear();
			definitions.clear();
			segments_mapping.clear();
		}

		friend std::ostream & operator<<(std::ostream & o, const SMTStore &v);
	};

	std::ostream & operator<<(std::ostream & o, const SMTStore &v);

}

