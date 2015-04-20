#include <algorithm>
#include <llvmsym/smtdatastore.h>
#include <toolkit/z3cache.h>

namespace llvm_sym {

unsigned SMTStore::unknown_instances = 0;

std::ostream & operator<<( std::ostream & o, const SMTStore &v )
{
    o << "data:\n";

    o << "\npath condition:\n";
    for ( const Formula &pc : v.path_condition )
        o << pc << '\n';

    o << "\ndefinitions:\n";
    for ( const Definition &def : v.definitions )
        o << def.to_formula() << "\n\n";
        
    return o;
}

bool SMTStore::empty() const
{
    ++Statistics::getCounter( STAT_EMPTY_CALLS );
    if ( path_condition.size() == 0 )
        return false;

    z3::context c;
    z3::expr pc( c );

    z3::solver s( c );

    for ( const Definition &def : definitions )
        s.add( toz3( def.to_formula(), 'a', c ) );

    for ( const Formula &pc : path_condition )
        s.add( toz3( pc, 'a', c ) );

    ++Statistics::getCounter( STAT_SMT_CALLS );
    z3::check_result r = s.check();

    assert( r != z3::unknown );

    return r == z3::unsat;
}

bool SMTStore::subseteq( const SMTStore &b, const SMTStore &a )
{
    ++Statistics::getCounter( STAT_SUBSETEQ_CALLS );
    if ( a.definitions == b.definitions ) {
        bool equal_syntax = a.path_condition.size() == b.path_condition.size();
        for ( size_t i = 0; equal_syntax && i < a.path_condition.size(); ++i ) {
            if ( a.path_condition[ i ]._rpn != b.path_condition[ i ]._rpn )
                equal_syntax = false;
        }
        if ( equal_syntax ) {
            ++Statistics::getCounter( STAT_SUBSETEQ_SYNTAX_EQUAL );
            return true;
        }
    }

    std::map< Formula::Ident, Formula::Ident > to_compare;
    for ( unsigned s = 0; s < a.generations.size(); ++s ) {
        assert( a.generations[ s ].size() == b.generations[ s ].size() );
        for ( unsigned offset = 0; offset < a.generations[ s ].size(); ++offset ) {
            Value var;
            var.type = Value::Type::Variable;
            var.variable.segmentId = s;
            var.variable.offset = offset;

            Formula::Ident a_atom = a.build_item( var );
            Formula::Ident b_atom = b.build_item( var );

            if ( !a.dependsOn( var ) && !b.dependsOn( var ) )
                continue;

            to_compare.insert( std::make_pair( a_atom, b_atom ) );
        }
    }

    if ( to_compare.empty() )
        return true;

    // pc_b && foreach(a).(!pc_a || a!=b)
    // (sat iff not _b_ subseteq _a_)
    z3::context c;
    z3::solver s( c );

    unsigned int timeout = 1 << (unknown_instances / 5);

    z3::params p( c );
    p.set(":mbqi", true);
    if (!Config.disabletimout.isSet())
        p.set("SOFT_TIMEOUT", timeout);
    s.set( p );

    bool is_caching_enabled = Config.enablecaching.isSet();
    Z3SubsetCall formula; // Structure for caching

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

        if (Config.verbose.isSet() || Config.vverbose.isSet())
            std::cout << "Building formula took " << s.getUs() << " us\n";

        // Test if this formula is in cache or not
        if (Z3cache.is_cached(formula))
            return Z3cache.result() == z3::unsat;
    }

    StopWatch solving_time;
    solving_time.start();

    z3::expr pc_a = c.bool_val( true );
    for (const auto &pc : a.path_condition)
        pc_a = pc_a && toz3(pc, 'a', c);
    z3::expr pc_b = c.bool_val( true );
    for (const auto &pc : b.path_condition)
        pc_b = pc_b && toz3(pc, 'b', c);

    for (const Definition &def : a.definitions)
        pc_a = pc_a && toz3(def.to_formula(), 'a', c);
    for (const Definition &def : b.definitions)
        pc_b = pc_b && toz3(def.to_formula(), 'b', c);

    z3::expr distinct = c.bool_val( false );

    for ( const auto &vars : to_compare ) {
        z3::expr a_expr = toz3( Formula::buildIdentifier( vars.first ), 'a', c );
        z3::expr b_expr = toz3( Formula::buildIdentifier( vars.second ), 'b', c );

        distinct = distinct || ( a_expr != b_expr );
    }

    std::vector< z3::expr > a_all_vars;

    for ( const auto &var : a.collectVaribles() ) {
        a_all_vars.push_back( toz3( Formula::buildIdentifier( var ), 'a', c ) );
    }

    z3::expr not_witness = !pc_a || distinct;
    s.add( pc_b );
    s.add( forall( a_all_vars, not_witness ) );

    //std::cerr << "checking equal():\n" << s << std::endl;

    ++Statistics::getCounter( STAT_SMT_CALLS );

    z3::check_result ret = s.check();
    if ( ret == z3::unknown ) {
        ++unknown_instances;
        if (Config.verbose.isSet() || Config.vverbose.isSet()) {
            if ( Config.vverbose.isSet() )
                std::cerr << "while checking:\n" << s;
            std::cerr << "\ngot 'unknown', reason: " << s.reason_unknown() << std::endl;
            std::cerr << "\ttimeout = " << timeout << std::endl;
        }
    }

    if ( ret == z3::sat )
        ++Statistics::getCounter( STAT_SUBSETEQ_SAT );
    else if ( ret == z3::unsat )
        ++Statistics::getCounter( STAT_SUBSETEQ_UNSAT );
    else
        ++Statistics::getCounter( STAT_SUBSETEQ_UNKNOWN );

    solving_time.stop();

    if (is_caching_enabled)
        Z3cache.place(formula, ret, solving_time.getUs());

    return ret == z3::unsat;
}

}

