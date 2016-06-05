#pragma once

#include <llvmsym/base_smt_datastore.h>
#include <llvmsym/smtdatastore.h>
#include <llvmsym/formula/rpn.h>
#include <llvmsym/formula/z3.h>
#include <llvmsym/programutils/statistics.h>
#include <llvmsym/programutils/config.h>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <toolkit/utils.h>



namespace llvm_sym {
    
class SMTStorePartial;

class SMTStorePartial : public BaseSMTStore<SMTStorePartial> {
    std::vector<short unsigned> segments_mapping;
    std::vector<std::vector<short unsigned>> generations;
    std::vector<std::vector<char>> bitWidths;
    
    bool test_run;
    SMTStore store;
    
    class dependency_group {
        std::set<Formula::Ident> group; 
        std::vector<Formula> path_condition;
        std::vector<Definition> definitions;
        TriState pc_state;
    public:
        dependency_group(const std::set<Formula::Ident>& g = {},
            const std::vector<Formula>& pc = {}, const std::vector<Definition>& d = {})
        : path_condition(pc), definitions(d), pc_state(TriState::UNKNOWN)
        {
            for (Formula::Ident id : g) {
                id.gen = 0;
                group.insert(id);
            }
        }
        
        bool operator<(const dependency_group& g) const {
            return group < g.group;
        }
        
        bool operator==(const dependency_group& g) const {
            return group == g.group;
        }
        
        void set_state(TriState s) {
            pc_state = s;
        }
        
        TriState get_state() const {
            return pc_state;
        }
        
        std::vector<Formula::Ident> collect_variables() const {
            std::vector<Formula::Ident> ret;
            collect_pc_variables(ret);
            collect_def_variables(ret);
            return ret;
        }
        
        void append(const dependency_group& g) {
            std::copy(g.group.cbegin(), g.group.cend(), std::inserter(group, group.end()));
            std::copy(g.path_condition.cbegin(), g.path_condition.cend(), std::back_inserter(path_condition));
            std::vector<Definition> new_defs;
            new_defs.reserve(definitions.size() + g.definitions.size());
            std::merge(definitions.begin(), definitions.end(),
                g.definitions.begin(), g.definitions.end(),
                std::back_inserter(new_defs));
            definitions = new_defs;
        }
        
        const std::set<Formula::Ident>& get_group() const {
            return group;
        }
        
        const std::vector<Formula>& get_path_condition() const {
            return path_condition;
        }
        
        const std::vector<Definition>& get_definitions() const {
            return definitions;
        }
        
        void push_condition(const Formula& formula) {
            pc_state = TriState::UNKNOWN;
            path_condition.push_back(std::move(formula));
        }
        
        void push_definition(const Definition& def) {
            auto it = std::upper_bound(definitions.begin(),
                definitions.end(), 
                def);
            definitions.insert(it, def);
        }
        
        void collect_pc_variables(std::vector<Formula::Ident>& v) const {
            for (const auto& pc : path_condition)
                pc.collect_variables(v);
        }
        
        void collect_def_variables(std::vector<Formula::Ident>& v) const {
            for (const auto& def : definitions)
                def.collect_variables(v);
        }
        
        bool depends_on(int seg, int offset, int generation) const {
            // ToDo: Can be simplified!
            // We could use a dependency group
	        //return group.find({ (unsigned short)seg, (unsigned short)offset, 0, 0 }) != group.end();
            for (const auto& pc : path_condition) {
                if (pc.depends_on(seg, offset, generation))
                    return true;
            }
            
            for (const auto& def : definitions) {
                if (def.depends_on(seg, offset, generation))
                    return true;
            }
            
            return false;
        }
        
        void simplify_pc() {
            if (path_condition.empty())
                return;
            Formula conj;
            for (const Formula& pc : path_condition)
                conj = conj && pc;
            
            if (Config.is_set("--cheapsimplify")) {
                auto simplified = cheap_simplify(conj);
                path_condition.resize(1);
                path_condition.back() = simplified;
            } 
            else if (!Config.is_set("--dontsimplify")) {
                auto simplified = llvm_sym::simplify(conj);
                path_condition.resize(1);
                path_condition.back() = simplified;
            }
        }
        
        size_t getSize() const {
            size_t size = 3 * sizeof(size_t) + group.size() * sizeof(Formula::Ident);
            size += sizeof(TriState);
            for (const auto& def : definitions) {
                size += representation_size(def.symbol) + representation_size(def.def._rpn);
            }
            
            for (const auto& pc : path_condition) {
                size += representation_size(pc._rpn);
            }
            
            return size;
        }
        
        void writeData(char * & mem) const {
            blobWrite(mem, definitions.size());
            for (const auto& def : definitions) {
                blobWrite(mem, def.symbol);
                blobWrite(mem, def.def._rpn);
            }
            
            blobWrite(mem, path_condition.size());
            for (const auto& pc : path_condition) {
                blobWrite(mem, pc._rpn);
            }
            
            blobWrite(mem, group.size());
            for (const auto& id : group) {
                blobWrite(mem, id);
            }
            
            blobWrite(mem, pc_state);
        }
        
        void readData(const char * & mem) {
            size_t def_size;
            blobRead(mem, def_size);
            definitions.resize(def_size);
            for (size_t i = 0; i < def_size; i++) {
                blobRead(mem, definitions[i].symbol);
                blobRead(mem, definitions[i].def._rpn);
            }
            
            size_t pc_size;
            blobRead(mem, pc_size);
            path_condition.resize(pc_size);
            for (size_t i = 0; i < pc_size; i++) {
                blobRead(mem, path_condition[i]._rpn);
            }
            
            size_t group_size;
            blobRead(mem, group_size);
            for (size_t i = 0; i != group_size; i++) {
                Formula::Ident id;
                blobRead(mem, id);
                group.insert(id);
            }
            
            blobRead(mem, pc_state);
        }
        
        template <typename Predicate>
        void removeDefinitions(Predicate pred) {
            // ToDo: Simplify dependency groups
            std::vector<Definition> to_remove(definitions.size());
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
            
            if (!to_remove.empty())
                pc_state = TriState::UNKNOWN;
        }
        
        template <typename Predicate>
        void removeConditions(Predicate pred) {
            auto it = std::remove_if(path_condition.begin(),
                path_condition.end(), pred);
            path_condition.resize(it - path_condition.begin());
            
            if (it != path_condition.end())
                pc_state = TriState::UNKNOWN;
        }
    };
     
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
                    get_generation( val.variable.segmentId, val.variable.offset ),
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
                        get_generation( val.variable.segmentId, val.variable.offset ),
                        bitWidths[ val.variable.segmentId ][ val.variable.offset ]
                        ) );
        }
    }

    Formula build_expression( Value val, bool advance_generation )
    {
        if(test_run)
            store.build_expression(val, advance_generation);
        if ( val.type == Value::Type::Constant )
            return Formula::buildConstant( val.constant.value, val.constant.bw );
        else {
            int segment_mapped_to = segments_mapping[ val.variable.segmentId ];
            return Formula::buildIdentifier( Formula::Ident (
                        segment_mapped_to,
                        val.variable.offset,
                        get_generation( val.variable.segmentId, val.variable.offset, advance_generation ),
                        bitWidths[ val.variable.segmentId ][ val.variable.offset ]
                        ) );
        }
    }

    int get_generation( unsigned segId, unsigned offset ) const
    {
        assert( segId < generations.size() );
        assert( offset < generations[ segId ].size() );
        return generations[ segId][ offset ];
    }

    int get_generation( unsigned segId, unsigned offset, bool advance_generation )
    {
        if (test_run)
            store.get_generation(segId, offset, advance_generation);
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
    
    int get_generation(Value val, bool advance_generation = false)
    {
        if (test_run)
            store.get_generation(val, advance_generation);
        assert( val.type == Value::Type::Variable );
        return get_generation( val.variable.segmentId, val.variable.offset, advance_generation );
    }
    
    dependency_group& resolve_dependency(const std::vector<Formula::Ident>& deps) {
        assert(!deps.empty());
        
        std::set<size_t> to_join;
        for (auto var : deps) {
            var.gen = 0;
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
            for (const auto& var : group.get_group())
                dependency_map[var] = res_id;            
            sym_data.erase(i);
        });
        
        return res;
    }

    void push_condition(const Formula &f)
    {
        if (test_run)
            store.push_condition(f);
        // Solve dependencies
        std::vector<Formula::Ident> deps;
        f.collect_variables(deps);
        
        auto& group = resolve_dependency(deps);
        
        group.push_condition(f);
        simplify();
    }

    void push_definition(Value symbol_id, const Formula &def)
    {
        if (test_run)
            store.push_definition(symbol_id, def);
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
        
        std::vector<Formula::Ident> deps;
        whole_def.collect_variables(deps);
        resolve_dependency(deps).push_definition(std::move(whole_def));
    }

    std::vector< Formula::Ident > collect_variables() const
    {
        std::vector<Formula::Ident> ret;
        
        for (const auto& group : sym_data) {
            group.second.collect_pc_variables(ret);
            group.second.collect_def_variables(ret);
        }

        return ret;
    }

    bool depends_on(Value symbol_id) const
    {
        int segment_mapped_to = segments_mapping[symbol_id.variable.segmentId];
        int offset = symbol_id.variable.offset;

        int gen = get_generation(symbol_id.variable.segmentId, offset);

        return depends_on(segment_mapped_to, offset, gen);
    }

    bool depends_on(int seg, int offset, int generation) const
    {
        for (const auto& group : sym_data) {
            if (group.second.depends_on(seg, offset, generation))
                return true;
        }
        return false;
    }

    public:
    
    SMTStorePartial() : test_run(Config.is_set("--testvalidity")) {
        
    }

    void simplify()
    {
        if (test_run)
            store.simplify();
        
        /*if (unknown_instances <= 4)
                // instances are very easy (solving time < 10ms), it is a waste to simplify
            return;*/

        for (auto& group : sym_data) {
            group.second.simplify_pc();
        }
    }

    virtual size_t getSize() const
    {
        int size = representation_size(segments_mapping, generations, bitWidths, fst_unused_id, test_run);
        
        // Sym data
        size += sizeof(size_t);
        for (const auto& group : sym_data) {
            size += representation_size(group.first); // Size of group ID
            size += group.second.getSize();
        }
        
        size += representation_size(sym_data.free_idx());
        
        // Dependency map
        size += sizeof(size_t);
        for (const auto& item : dependency_map) {
            size += representation_size(item.first);
            size += representation_size(item.second);
        }

        return test_run ? size + store.getSize() : size;
    }

    virtual void writeData(char *&mem) const
    {
        blobWrite(mem, segments_mapping, generations, bitWidths, fst_unused_id);
        
        blobWrite(mem, sym_data.size());
        for (const auto& group : sym_data) {
            blobWrite(mem, group.first);
            group.second.writeData(mem); 
        }
        blobWrite(mem, sym_data.free_idx());
        
        blobWrite(mem, dependency_map.size());       
        for (const auto& item : dependency_map) {
            blobWrite(mem, item.first);
            blobWrite(mem, item.second);
        }
        blobWrite(mem, test_run);
        if (test_run) {
            store.writeData(mem);
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
            group.readData(mem);
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
        blobRead(mem, test_run);
        if (test_run)
            store.readData(mem);
    }

    void pushPropGuard(const Formula &g) {
        if (test_run)
            store.pushPropGuard(g);
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

    virtual void addSegment( unsigned id, const std::vector< int > &bit_widths )
    {
        if (test_run)
            store.addSegment(id, bit_widths);
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
        if (test_run)
            store.eraseSegment(id);
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
        if (test_run)
            store.removeDefinitions(pred);
        // ToDo: Simplify dependency groups!
        for (auto& group : sym_data) {
            group.second.removeDefinitions(pred);
        }
    }

    template < typename Predicate >
    void removeConditions( Predicate pred )
    {
        if (test_run)
            store.removeConditions(pred);
        // precondition: run removeDefinition( pred ) to be sure that the symbols
        // are not used anywhere else
        for (auto& group : sym_data) {
            group.second.removeConditions(pred);
        }
    }

	bool equal(const SMTStorePartial &snd)
    {
		assert(segments_mapping.size() == snd.segments_mapping.size());
        static bool timeout = !Config.is_set("--disabletimeout");
        static bool cache = Config.is_set("--enablecaching");
		return subseteq(*this, snd, timeout, cache) &&
               subseteq(snd, *this, timeout, cache);
	}

    virtual bool empty();

	static bool subseteq(const SMTStorePartial &a, const SMTStorePartial &b,
        bool timeout, bool cache);
    static bool subseteq(
        const std::vector<std::reference_wrapper<const dependency_group>>& a_g,
        const std::vector<std::reference_wrapper<const dependency_group>>& b_g,
        const std::map<Formula::Ident, Formula::Ident>& to_compare, bool timeout,
        bool cache);
    
    static bool syntax_equal(const dependency_group a, const dependency_group b);

    void clear()
    {
        dependency_map.clear();
        segments_mapping.clear();
    }

    friend std::ostream & operator<<( std::ostream & o, const SMTStorePartial &v );
    friend std::ostream& operator<<(std::ostream& o, const SMTStorePartial::dependency_group& g);
};

std::ostream & operator<<( std::ostream & o, const SMTStorePartial &v );
std::ostream& operator<<(std::ostream& o, const SMTStorePartial::dependency_group& g);

}

