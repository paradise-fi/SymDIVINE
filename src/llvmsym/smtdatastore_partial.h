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

#define STAT_SUBSETEQ_CALLS "SMT calls Subseteq()"
#define STAT_EMPTY_CALLS "SMT calls Empty()"
#define STAT_SMT_CALLS "SMT queries"
#define STAT_SUBSETEQ_SAT "SMT queries: SAT"
#define STAT_SUBSETEQ_UNSAT "SMT queries: unSAT"
#define STAT_SUBSETEQ_UNKNOWN "SMT queries: unSAT"
#define STAT_SUBSETEQ_SYNTAX_EQUAL "SMT subseteq on syntax. equal"
#define STAT_SMT_SIMPLIFY_CALLS "SMT simplify calls"

namespace llvm_sym {

class SMTStorePartial : public DataStore {
    std::vector<short unsigned> segments_mapping;
    std::vector<std::vector<short unsigned>> generations;
    std::vector<std::vector<char>> bitWidths;
    
    struct dependency_group {
        std::set<Formula::Ident> group; // ToDo: Think about using std::vector for speed increase
        std::vector<Formula> path_condition;
        std::vector<Definition> definitions;
        
        dependency_group(const std::set<Formula::Ident>& g = {},
            const std::vector<Formula>& pc = {}, const std::vector<Definition>& d = {})
        : group(g), path_condition(pc), definitions(d) {}
        
        bool operator<(const dependency_group& g) const {
            return group < g.group;
        }
        
        bool operator==(const dependency_group& g) const {
            return group == g.group;
        }
        
        void append(const dependency_group& g) {
            std::copy(g.group.cbegin(), g.group.cend(), std::inserter(group, group.end()));
            std::copy(g.path_condition.cbegin(), g.path_condition.cend(), std::back_inserter(path_condition));
            std::copy(g.definitions.cbegin(), g.definitions.cend(), std::back_inserter(definitions));
        }
    };
    
    using dep_pointer = std::list<dependency_group>::iterator;
    using dep_pointer_const = std::list<dependency_group>::const_iterator;
    
    std::map<Formula::Ident, size_t> dependency_map; // Identifier map
    IdContainer<dependency_group> sym_data; // path_condition & definition
    
    int fst_unused_id = 0;
    static unsigned unknown_instances;

    Formula::Ident build_item( Value val ) const
    {
        assert ( val.type == Value::Type::Variable );
        int segment_mapped_to = segments_mapping[ val.variable.segmentId ];
        return Formula::Ident (
                    segment_mapped_to,
                    val.variable.offset,
                    getGeneration( val.variable.segmentId, val.variable.offset ),
                    bitWidths[ val.variable.segmentId ][ val.variable.offset ]
                    );
    }

    Formula build_expression( Value val ) const
    {
        if ( val.type == Value::Type::Constant )
            return Formula::buildConstant( val.constant.value, val.constant.bw );
        else {
            int segment_mapped_to = segments_mapping[ val.variable.segmentId ];
            return Formula::buildIdentifier( Formula::Ident (
                        segment_mapped_to,
                        val.variable.offset,
                        getGeneration( val.variable.segmentId, val.variable.offset ),
                        bitWidths[ val.variable.segmentId ][ val.variable.offset ]
                        ) );
        }
    }

    Formula build_expression( Value val, bool advance_generation )
    {
        if ( val.type == Value::Type::Constant )
            return Formula::buildConstant( val.constant.value, val.constant.bw );
        else {
            int segment_mapped_to = segments_mapping[ val.variable.segmentId ];
            return Formula::buildIdentifier( Formula::Ident (
                        segment_mapped_to,
                        val.variable.offset,
                        getGeneration( val.variable.segmentId, val.variable.offset, advance_generation ),
                        bitWidths[ val.variable.segmentId ][ val.variable.offset ]
                        ) );
        }
    }

    int getGeneration( unsigned segId, unsigned offset ) const
    {
        assert( segId < generations.size() );
        assert( offset < generations[ segId ].size() );
        return generations[ segId][ offset ];
    }

    int getGeneration( unsigned segId, unsigned offset, bool advance_generation )
    {
        assert( segId < generations.size() );
        assert( offset < generations[ segId ].size() );
        short unsigned &g = generations[ segId ][ offset ];
        const int seg_m = segments_mapping[ segId ];
        if ( advance_generation ) {
            const auto &is_variable_predicate = [offset, g, seg_m]( const Definition &d ) {
                return d.isInSegment( seg_m ) && d.isOffset( offset ) && d.isGeneration( g );
            };
            removeDefinitions( is_variable_predicate );
            ++g;
        }

        return g;
    }
    
    int getGeneration(Value val, bool advance_generation = false)
    {
        assert( val.type == Value::Type::Variable );
        return getGeneration( val.variable.segmentId, val.variable.offset, advance_generation );
    }
    
    dependency_group& resolve_dependency(const std::vector<Formula::Ident>& deps) {
        assert(!deps.empty());
        
        std::set<size_t> to_join;
        for (const auto& var : deps) {
            auto res = dependency_map.find(var);
            if (res == dependency_map.end()) {
                // This variable doesn't exist - create it!
                dependency_group inf({var}, {}, {});
                size_t it = sym_data.insert(inf);

                dependency_map.insert({var, it});
                to_join.insert(it);
            }
            else {
                to_join.insert(res->second);
            }
        }
        assert(!to_join.empty());
        
        // Merge!
        dependency_group& res = sym_data.get(*to_join.begin());
        size_t res_id = *to_join.begin();
        for_each(++to_join.begin(), to_join.end(), [&](size_t i) {
            auto& group = sym_data.get(i);
            res.append(group);
            for (const auto& var : group.group)
                dependency_map[var] = res_id;            
            sym_data.erase(i);
        });
        
        return res;
    }

    void pushCondition(const Formula &f)
    {
        // Solve dependencies
        std::vector<Formula::Ident> deps;
        f.collectVariables(deps);
        
        auto& group = resolve_dependency(deps);
        
        group.path_condition.push_back(f);
        simplify();
    }

    void pushDefinition(Value symbol_id, const Formula &def)
    {
        assert(def.sane());
        assert(!def._rpn.empty());
        int segment_mapped_to = segments_mapping[symbol_id.variable.segmentId];
        auto ident = Formula::Ident(
                        segment_mapped_to,
                        symbol_id.variable.offset,
                        getGeneration(symbol_id.variable.segmentId, symbol_id.variable.offset, true),
                        bitWidths[symbol_id.variable.segmentId][symbol_id.variable.offset]
                );
        const Definition whole_def = Definition(ident, def);
        
        std::vector<Formula::Ident> deps;
        whole_def.collectVariables(deps);
        
        auto& info = resolve_dependency(deps);
        
        auto it = std::upper_bound(info.definitions.begin(),info.definitions.end(), whole_def);
        info.definitions.insert(it, whole_def);
    }

    std::vector< Formula::Ident > collectVaribles() const
    {
        std::vector<Formula::Ident> ret;
        
        for (const auto& group : sym_data) {
            for (const auto& pc : group.second.path_condition)
                pc.collectVariables(ret);
            for (const auto& def : group.second.definitions)
                def.collectVariables(ret);
        }

        return ret;
    }

    bool dependsOn(Value symbol_id) const
    {
        int segment_mapped_to = segments_mapping[symbol_id.variable.segmentId];
        int offset = symbol_id.variable.offset;

        int gen = getGeneration(symbol_id.variable.segmentId, offset);

        return dependsOn(segment_mapped_to, offset, gen);
    }

    bool dependsOn(int seg, int offset, int generation) const
    {
        for (auto& group : sym_data) {
            for (const auto& pc : group.second.path_condition) {
                if (pc.dependsOn(seg, offset, generation))
                    return true;
            }
            for (const auto& def : group.second.definitions) {
                if (def.dependsOn(seg, offset, generation))
                    return true;
            }
        }
    }

    public:

    void simplify()
    {
        // ToDo: Add dependency simplification
        ++Statistics::getCounter(STAT_SMT_SIMPLIFY_CALLS);
        
        if (unknown_instances <= 4)
                // instances are very easy (solving time < 10ms), it is a waste to simplify
            return;

        for (auto& group : sym_data) {
            if (group.second.path_condition.empty())
                continue;
            Formula conj;
            for (Formula &pc : group.second.path_condition)
                conj = conj && pc;

            if (Config.is_set("--cheapsimplify")) {
                auto simplified = cheap_simplify(conj);
                group.second.path_condition.resize(1);
                group.second.path_condition.back() = simplified;
            }
            else if (!Config.is_set("--dontsimplify")) {
                // regular, full, expensive simplify
                auto simplified = llvm_sym::simplify(conj);
                group.second.path_condition.resize(1);
                group.second.path_condition.back() = simplified;
            }
        }
    }

    virtual size_t getSize() const
    {
        int size = representation_size(segments_mapping, generations, bitWidths, fst_unused_id);
        
        // Sym data
        size += sizeof(size_t);
        for (const auto& group : sym_data) {
            size += representation_size(group.first); // Size of group ID
            
            size += sizeof(size_t);
            for ( const Definition &f : group.second.definitions ) {
                size += representation_size(f.symbol);
                size += representation_size(f.def._rpn);
            }
            size += sizeof(size_t);
            for (const Formula &pc : group.second.path_condition) {
                size += representation_size(pc._rpn);
            }
            size += sizeof(size_t);
            for (const Formula::Ident& id : group.second.group) {
                size += representation_size(id);
            }
        }
        
        size += representation_size(sym_data.free_idx());
        
        // Dependency map
        size += sizeof(size_t);
        for (const auto& item : dependency_map) {
            size += representation_size(item.first);
            size += representation_size(item.second);
        }

        return size;
    }

    virtual void writeData(char *&mem) const
    {
        blobWrite(mem, segments_mapping, generations, bitWidths, fst_unused_id);
        
        blobWrite(mem, sym_data.size());
        for (const auto& group : sym_data) {
            blobWrite(mem, group.first);
            
            blobWrite(mem, group.second.definitions.size());
            for (const Definition &f : group.second.definitions) {
                blobWrite(mem, f.symbol);
                blobWrite(mem, f.def._rpn);
            }
            blobWrite(mem, group.second.path_condition.size());
            for (const Formula &pc : group.second.path_condition) {
                blobWrite(mem, pc._rpn);
            }
            
            blobWrite(mem, group.second.group.size());
            for (const Formula::Ident &id : group.second.group) {
                blobWrite(mem, id);
            }
        }
        blobWrite(mem, sym_data.free_idx());
        
        blobWrite(mem, dependency_map.size());       
        for (const auto& item : dependency_map) {
            blobWrite(mem, item.first);
            blobWrite(mem, item.second);
        }
    }
    
    virtual void readData(const char * &mem)
    {
        blobRead(mem, segments_mapping, generations, bitWidths, fst_unused_id);

        sym_data.clear();
        size_t data_size;
        blobRead(mem, data_size);
        for (size_t i = 0; i != data_size; i++) {
            size_t id;
            blobRead(mem, id);
            dependency_group& group = sym_data.insert(id, dependency_group());
            
            size_t definitions_size;
            blobRead(mem, definitions_size);
            group.definitions.resize(definitions_size);
            for (unsigned i = 0; i < definitions_size; ++i) {
                blobRead(mem, group.definitions[i].symbol);
                blobRead(mem, group.definitions[i].def._rpn);
            }
            
            size_t pc_size;
            blobRead(mem, pc_size);
            group.path_condition.resize(pc_size);
            for (unsigned i = 0; i < pc_size; ++i) {
                blobRead(mem, group.path_condition[i]._rpn);
            }
            
            size_t group_size;
            blobRead(mem, group_size);
            for (size_t i = 0; i != group_size; i++) {
                Formula::Ident id;
                blobRead(mem, id);
                group.group.insert(id);
            }
        }
        sym_data.free_idx().clear();
        blobRead(mem, sym_data.free_idx());
        
        dependency_map.clear();
        size_t dep_size;
        blobRead(mem, dep_size);
        for (size_t i = 0; i != dep_size; i++) {
            Formula::Ident id;
            size_t index;
            blobRead(mem, id);
            blobRead(mem, index);
            dependency_map.insert({id, index});
        }   
        
        assert(segments_mapping.size() == generations.size());
        assert(segments_mapping.size() == bitWidths.size());
    }

    void pushPropGuard(const Formula &g) {
        Formula f = g;
        std::vector<Formula::Ident> ids;
        for (auto &i : f._rpn) {
            if (i.kind == Formula::Item::Kind::Identifier) {
                i.id.gen = getGeneration(i.id.seg, i.id.off, false);
                i.id.bw = bitWidths[i.id.seg][i.id.off];
            }
        }
        pushCondition(f);
    }
    
    int getBitWidth(Value val) {
        return bitWidths[val.variable.segmentId][val.variable.offset];
    }

    virtual void implement_add( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr + b_expr );
    }

    virtual void implement_mult( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr * b_expr );
    }

    virtual void implement_sub( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr - b_expr );
    }

    virtual void implement_div( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr / b_expr );
    }
    
    virtual void implement_urem( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr.buildURem( b_expr ) );
    }
    
    virtual void implement_srem( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr.buildSRem( b_expr ) );
    }

    virtual void implement_and( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr & b_expr );
    }

    virtual void implement_or( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr | b_expr );
    }

    virtual void implement_xor( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr ^ b_expr );
    }

    virtual void implement_left_shift( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr << b_expr );
    }

    virtual void implement_right_shift( Value result_id, Value a_id, Value b_id )
    {
        Formula a_expr = build_expression( a_id );
        Formula b_expr = build_expression( b_id );
        pushDefinition( result_id, a_expr >> b_expr );
    }

    virtual void implement_store( Value result_id, Value what )
    {
        Formula what_expr = build_expression( what );
        pushDefinition( result_id, what_expr );
    }

    virtual void implement_ZExt( Value result_id, Value a_id, int bw )
    {
        Formula what_expr = build_expression( a_id );
        pushDefinition( result_id, what_expr.buildZExt( bw ) );
    }

	virtual void implement_SExt(Value result_id, Value a_id, int bw) {
		Formula what_expr = build_expression(a_id);
		pushDefinition(result_id, what_expr.buildSExt(bw));
	}

    virtual void implement_Trunc( Value result_id, Value a_id, int bw )
    {
        Formula what_expr = build_expression( a_id );
        pushDefinition( result_id, what_expr.buildTrunc( bw - 1, 0 ) );
    }
    
    virtual void implement_inttoptr(Value result_id, Value a_id) {
        Formula what_expr = build_expression(a_id);
        pushDefinition(result_id, what_expr);
    }
    
    virtual void implement_ptrtoint(Value result_id, Value a_id) {
        Formula what_expr = build_expression(a_id);
        pushDefinition(result_id, what_expr);
    }

    virtual void prune( Value a, Value b, ICmp_Op op )
    {
        Formula a_expr = build_expression( a );
        Formula b_expr = build_expression( b );
        switch ( op ) {
            case ICmp_Op::EQ:
                pushCondition( a_expr == b_expr );
                break;
            case ICmp_Op::NE:
                pushCondition( a_expr != b_expr );
                break;
            case ICmp_Op::UGT:
                pushCondition( a_expr.buildUGT( b_expr ) );
                break;
            case ICmp_Op::SGT:
                pushCondition( a_expr > b_expr );
                break;
            case ICmp_Op::UGE:
                pushCondition( a_expr.buildUGEq( b_expr ) );
                break;
            case ICmp_Op::SGE:
                pushCondition( a_expr >= b_expr );
                break;
            case ICmp_Op::ULT:
                pushCondition( a_expr.buildULT( b_expr ) );
                break;
            case ICmp_Op::SLT:
                pushCondition( a_expr < b_expr );
                break;
            case ICmp_Op::ULE:
                pushCondition( a_expr.buildULEq( b_expr ) );
                break;
            case ICmp_Op::SLE:
                pushCondition( a_expr <= b_expr );
                break;
        }
    }

    virtual void implement_input( Value input_variable, unsigned bw )
    {
        getGeneration( input_variable, true );
    }

    virtual void addSegment( unsigned id, const std::vector< int > &bit_widths )
    {
        segments_mapping.insert( segments_mapping.begin() + id, fst_unused_id++ );
        generations.emplace( generations.begin() + id, bit_widths.size(), 0 );
        bitWidths.insert( bitWidths.begin() + id, std::vector< char >( bit_widths.size() ) );

        for ( size_t i = 0; i < bit_widths.size(); ++i ) {
            bitWidths[ id ][ i ] = static_cast< char >( bit_widths[ i ] );
        }

        for ( unsigned offset = 0; offset < bit_widths.size(); ++offset ) {
            Value new_val;
            new_val.type = Value::Type::Variable;
            new_val.variable.segmentId = id;
            new_val.variable.offset = offset;
            //implement_store( new_val, Value( 0, bit_widths[ offset ] ) );
        }
    }

    virtual void eraseSegment( int id )
    {
        int mapped_to = segments_mapping[ id ];
        const auto &in_segment_predicate = [mapped_to]( const Definition &d ) {
            return d.isInSegment( mapped_to );
        };
        removeDefinitions( in_segment_predicate );
        //removeConditions( in_segment_predicate_f );
        segments_mapping.erase( segments_mapping.begin() + id );
        generations.erase( generations.begin() + id );
        bitWidths.erase( bitWidths.begin() + id );
        simplify();
    }

    template < typename Predicate >
    void removeDefinitions( Predicate pred )
    {
        // ToDo: Simplify dependency groups!
        for (auto& group : sym_data) {
            std::vector< Definition > to_remove(group.second.definitions.size());
            auto it = std::copy_if(group.second.definitions.begin(), group.second.definitions.end(), to_remove.begin(), pred);
            to_remove.resize(it - to_remove.begin());

            it = std::remove_if(group.second.definitions.begin(), group.second.definitions.end(), pred);
            group.second.definitions.resize(it - group.second.definitions.begin());

            bool change;
            do {
                change = false;
                for (Formula &pc : group.second.path_condition) {
                    for (const Definition &def : to_remove) {
                        Formula new_pc = pc.substitute(def.getIdent(), def.getDef());
                        if (new_pc._rpn != pc._rpn) {
                            change = true;
                            pc = new_pc;
                        }
                    }
                }

                for (Definition &def : group.second.definitions) {
                    for (auto def_to_remove = to_remove.crbegin(); def_to_remove != to_remove.crend(); ++def_to_remove) {
                        Definition new_def = def.substitute(def_to_remove->getIdent(), def_to_remove->getDef());
                        if (new_def.def._rpn != def.def._rpn) {
                            def = def.substitute(def_to_remove->getIdent(), def_to_remove->getDef());
                            change = true;
                        }
                    }
                }
            } while (change);
            std::sort(group.second.definitions.begin(), group.second.definitions.end());
        }
    }

    template < typename Predicate >
    void removeConditions( Predicate pred )
    {
        // precondition: run removeDefinition( pred ) to be sure that the symbols
        // are not used anywhere else
        for (auto& group : sym_data) {
            auto it = std::remove_if(group.second.path_condition.begin(),
                group.second.path_condition.end(), pred);
            group.second.path_condition.resize(it - group.second.path_condition.begin());
        }
    }

	bool equal(const SMTStorePartial &snd)
    {
		assert(segments_mapping.size() == snd.segments_mapping.size());

		return subseteq(*this, snd) && subseteq(snd, *this);
	}

    virtual bool empty() const;

	static bool subseteq(const SMTStorePartial &a, const SMTStorePartial &b);
    static bool subseteq(
        const std::vector<std::reference_wrapper<const dependency_group>>& a_g,
        const std::vector<std::reference_wrapper<const dependency_group>>& b_g);

    void clear()
    {
        dependency_map.clear();
        segments_mapping.clear();
    }

    friend std::ostream & operator<<( std::ostream & o, const SMTStorePartial &v );
};

std::ostream & operator<<( std::ostream & o, const SMTStorePartial &v );

}

