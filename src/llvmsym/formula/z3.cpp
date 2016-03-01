#include <llvmsym/formula/z3.h>
#include <llvmsym/programutils/config.h>
#include <z3.h>

namespace llvm_sym {

namespace {
    void operator2z3unary( const Formula::Item &i, std::vector< z3::expr > &res, z3::context &c )
    {
        assert( res.size() >= 1 );
        z3::expr l = res.back();
        res.pop_back();
    
        switch ( i.op ) {
            case Formula::Item::Not:
                res.push_back( !l );
                break;
            case Formula::Item::ZExt:
                res.push_back( z3::expr( c,
                            Z3_mk_zero_ext( c, i.value - l.get_sort().bv_size(), l ) ) );
                break;
            case Formula::Item::SExt:
                res.push_back( z3::expr( c,
                            Z3_mk_sign_ext( c, i.value - l.get_sort().bv_size(), l ) ) );
                break;
            case Formula::Item::Trunc:
                res.push_back( z3::expr( c,
                            Z3_mk_extract( c, i.value - 1, 0, l ) ) );
                break;
            case Formula::Item::BNot:
                res.push_back( z3::expr( c, Z3_mk_bvnot( c, l ) ) );
                break;
            default:
                abort();
        }
    }
    void operator2z3( const Formula::Item &i, std::vector< z3::expr > &res, z3::context &c )
    {
        assert( res.size() >= 2 );
        z3::expr r = res.back();
        res.pop_back();
        z3::expr l = res.back();
        res.pop_back();
        
        switch ( i.op ) {
            case Formula::Item::Plus:
                res.push_back( l + r );
                break;
            case Formula::Item::Minus:
                res.push_back( l - r );
                break;
            case Formula::Item::Times:
                res.push_back( l * r );
                break;
            case Formula::Item::Div:
                res.push_back( l / r );
                break;
            case Formula::Item::Eq:
                res.push_back( l == r );
                break;
            case Formula::Item::URem:
                res.push_back( z3::expr( c, Z3_mk_bvurem( c, l, r ) ) );
                break;
            case Formula::Item::SRem:
                res.push_back( z3::expr( c, Z3_mk_bvsrem( c, l, r ) ) );
                break;
            case Formula::Item::NEq:
                res.push_back( l != r );
                break;
            case Formula::Item::And:
                res.push_back( l && r );
                break;
            case Formula::Item::Or:
                res.push_back( l || r );
                break;
            case Formula::Item::BAnd:
                res.push_back( l & r );
                break;
            case Formula::Item::Shl:
                res.push_back( z3::expr( c, Z3_mk_bvshl( c, l, r ) ) );
                break;
            case Formula::Item::Shr:
                res.push_back( z3::expr( c, Z3_mk_bvlshr( c, l, r ) ) );
                break;
            case Formula::Item::Concat:
                res.push_back( z3::expr( c, Z3_mk_concat( c, l, r ) ) );
                break;
            case Formula::Item::BOr:
                res.push_back( l | r );
                break;
            case Formula::Item::Xor:
                res.push_back( l ^ r );
                break;
            case Formula::Item::LT:
                res.push_back( l < r );
                break;
            case Formula::Item::GT:
                res.push_back( l > r );
                break;
            case Formula::Item::LEq:
                res.push_back( l <= r );
                break;
            case Formula::Item::GEq:
                res.push_back( l >= r );
                break;
            case Formula::Item::ULT:
                res.push_back( z3::ult( l, r ) );
                break;
            case Formula::Item::UGT:
                res.push_back( z3::ugt( l, r ) );
                break;
            case Formula::Item::ULEq:
                res.push_back( z3::ule( l, r ) );
                break;
            case Formula::Item::UGEq:
                res.push_back( z3::uge( l, r ) );
                break;
            case Formula::Item::ZExt:
                res.push_back( z3::expr( c,
                            Z3_mk_zero_ext( c, i.value - l.get_sort().bv_size(), l ) ) );
                break;
            case Formula::Item::SExt:
                res.push_back( z3::expr( c,
                            Z3_mk_sign_ext( c, i.value - l.get_sort().bv_size(), l ) ) );
                break;
            case Formula::Item::Trunc:
                res.push_back( z3::expr( c,
                            Z3_mk_extract( c, i.value - 1, 0, l ) ) );
                break;
            default:
                abort();
        }
    }

    void constant2z3( const Formula::Item &i, std::vector< z3::expr > &res, z3::context &c )
    {
        res.push_back( c.bv_val( i.value, i.id.bw ) );
    }
    
    void bool2z3( const Formula::Item &i, std::vector< z3::expr > &res, z3::context &c )
    {
        res.push_back( c.bool_val( i.value == 1 ) );
    }

    void identifier2z3( const Formula::Item &i, std::vector< z3::expr > &res, z3::context &c, char vpref )
    {
        std::stringstream name;
        name << vpref << "_seg" << i.id.seg
             << "_off" << i.id.off
             << "_gen" << i.id.gen;
        res.push_back( c.bv_const( name.str().c_str(), i.id.bw ) );
    }

    void rpn2z3( const Formula &f, std::vector< z3::expr > &res, z3::context &c, char vpref )
    {
        for ( unsigned pos = 0; pos < f.size(); ++pos ) {
            switch ( f.at( pos ).kind ) {
                case Formula::Item::Op:
                    if ( f.at( pos ).is_unary_op() )
                        operator2z3unary( f.at( pos ), res, c );
                    else
                        operator2z3( f.at( pos ), res, c );
                    break;
                case Formula::Item::Constant:
                    constant2z3( f.at( pos ), res, c );
                    break;
                case Formula::Item::Identifier:
                    identifier2z3( f.at( pos ), res, c, vpref );
                    break;
                case Formula::Item::BoolVal:
                    bool2z3( f.at( pos ), res, c );
                    break;
            }
        }
    }
}

const Formula fromz3()
{
    return Formula();
}

const Formula fromz3( const z3::expr &expr )
{
    expr.check_error();
    std::stringstream ss;

    bool is_variable = expr.is_const() && !expr.is_numeral() && !expr.is_bool();
    bool is_bool = expr.is_const() && expr.is_bool();
    bool is_bv = expr.is_const() && !expr.is_bool() && expr.is_numeral();
    bool is_app = !is_variable && !expr.is_const() && expr.is_app();
    char dummy[8];

    assert( is_variable + is_bool + is_bv + is_app == 1 );

    if ( is_variable ) {
        int segment, offset, generation, bw;
        ss << expr.decl().name();
        ss.read( dummy, 5 ); // a_seg
        ss >> segment;
        ss.read( dummy, 4 ); // _off
        assert( std::string( dummy, 4 ) == "_off" );
        ss >> offset;
        ss.read( dummy, 4 ); // _gen
        assert( std::string( dummy, 4 ) == "_gen" );
        ss >> generation;
        bw = expr.decl().range().bv_size();
        return Formula::buildIdentifier( Formula::Ident( segment, offset, generation, bw ) );
    } else if ( is_bv ) {
        ss << expr;
        int n;
        ss.read( dummy, 2 ); // #x
        ss << std::hex;
        assert( expr.decl().is_const() );
        int bw = expr.decl().range().bv_size();
        ss >> n;
        return Formula::buildConstant( n, bw );
    } else if ( is_bool ) {
        ss << expr;
        assert( ss.str() == "true" || ss.str() == "false" );
        return Formula::buildBoolVal( ss.str() == "true" );
    } else if ( is_app ) {
        switch( expr.num_args() ) {
            case 2: {
                Formula l = fromz3( expr.arg( 0 ) ), r = fromz3( expr.arg( 1 ) );
                switch ( expr.decl().decl_kind() ) {
                    case Z3_OP_BADD:    return l + r;
                    case Z3_OP_BMUL:    return l * r;
                    case Z3_OP_BSUB:    return l - r;
                    case Z3_OP_BSDIV:
                    case Z3_OP_BUDIV:   return l / r;
                    case Z3_OP_ULEQ:    return l.buildULEq( r );
                    case Z3_OP_SLEQ:    return l <= r;
                    case Z3_OP_UGEQ:    return l.buildUGEq( r );
                    case Z3_OP_SGEQ:    return l >= r;
                    case Z3_OP_ULT:     return l.buildULT( r );
                    case Z3_OP_SLT:     return l < r;
                    case Z3_OP_UGT:     return l.buildUGT( r );
                    case Z3_OP_SGT:     return l > r;
                    case Z3_OP_BSREM:   return l.buildSRem( r );
                    case Z3_OP_BUREM:   return l.buildURem( r );
                    case Z3_OP_EQ:      return l == r;
                    case Z3_OP_DISTINCT:return l != r;
                    case Z3_OP_AND:     return l && r;
                    case Z3_OP_BAND:    return l & r;
                    case Z3_OP_OR:      return l || r;
                    case Z3_OP_BOR:     return l | r;
                    case Z3_OP_BSHL:    return l << r;
                    case Z3_OP_BLSHR:   return l >> r;
                    case Z3_OP_CONCAT:  return l.buildConcat( r );
                    default:
                        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
                            std::cerr << "fromz3: unknown binary expression type "
                                      << std::hex << expr.decl().decl_kind() << " of "
                                      << expr << std::endl;
                        throw std::exception();
                }
            }
            case 1: {
                Formula l = fromz3( expr.arg( 0 ) );
                switch ( expr.decl().decl_kind() ) {
                    case Z3_OP_SIGN_EXT: return l.buildSExt( expr.get_sort().bv_size() );
                    case Z3_OP_ZERO_EXT: return l.buildZExt( expr.get_sort().bv_size() );
                    case Z3_OP_EXTRACT:  return l.buildTrunc( expr.get_sort().bv_size() );
                    case Z3_OP_NOT:      return !l;
                    case Z3_OP_BNOT:     return l.buildBNot();
                    default:
                        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
                            std::cerr << "fromz3: unknown unary expression type "
                                      << std::hex << expr.decl().decl_kind() << "of "
                                      << expr << std::endl;
                        throw std::exception();
                }
            }
            if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
                std::cerr << "fromz3: unknown expression appl " << expr
                          << " with " << expr.num_args() << " args" << std::endl;
            throw std::exception();
        }
    }

    if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
        std::cerr << "fromz3: unknown expression type of " << expr << std::endl;
    throw std::exception();
}

z3::expr toz3( const Formula &f, char vpref, z3::context &c )
{
    try {
        assert(f.sane());
        std::vector< z3::expr > stack;
        rpn2z3(f, stack, c, vpref);
   
        return stack.empty() ? c.bool_val(true) : stack[0];
    }
    catch (const z3::exception& e) {
        std::cerr << "toz3: invalid formula " << f << "\n";
        throw e;
    }
}

z3::expr forall( const std::vector< z3::expr > &v, const z3::expr & b )
{
    std::vector< Z3_app > vars;
    for ( unsigned i = 0; i < v.size(); i++ ) {
        check_context( v.at( i ), b );
        vars.push_back( (Z3_app) v.at( i ) );
    }
    Z3_ast r = Z3_mk_forall_const(b.ctx(), 0, v.size(), vars.data(), 0, 0, b);
    b.check_error();
    return z3::expr(b.ctx(), r);
}

z3::expr exists( const std::vector< z3::expr > &v, const z3::expr & b )
{
    std::vector< Z3_app > vars;
    for ( unsigned i = 0; i < v.size(); i++ ) {
        check_context( v.at( i ), b );
        vars.push_back( (Z3_app) v.at( i ) );
    }
    Z3_ast r = Z3_mk_exists_const(b.ctx(), 0, v.size(), vars.data(), 0, 0, b);
    b.check_error();
    return z3::expr(b.ctx(), r);
}

Formula simplify( const z3::expr f, std::string tactics )
{
    z3::context &ctx = f.ctx();

    z3::tactic simplifier( ctx, tactics.c_str() );
    z3::goal goal_f( ctx );
    goal_f.add( f );
    z3::apply_result result = simplifier.apply( goal_f );

    z3::expr result_expr = z3::expr( ctx );
    bool empty = true;

    for ( unsigned g = 0; g < result.size(); ++g ) {
        for ( unsigned e = 0; e < result[ g ].size(); ++e ) {
            if ( empty )
                result_expr = result[ g ][ e ];
            else 
                result_expr = result_expr && result[ g ][ e ];
            empty = false;
        }
    }

    if ( empty )
        return fromz3();

    return fromz3( result_expr );
}

Formula simplify( const Formula &f )
{
    if ( f.size() == 0 )
        return f;
    z3::context ctx;
    
    try {
        return simplify(toz3(f, 'a', ctx), "ctx-solver-simplify");
    }
    catch (std::exception e) {
        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
            std::cerr << "continuing with non-simplified formula " << f << std::endl;
        return f;
    }
    catch (const z3::exception& e) {
        std::cerr << "Warning: z3 simplify failed, continuing with non-simplified"
            " formula " << f << std::endl;
        abort();
    }

}

Formula cheap_simplify( const Formula &f )
{
    if ( f.size() == 0 )
        return f;
    z3::context ctx;
    try {
        return simplify( toz3( f, 'a', ctx ), "ctx-simplify" );
    } catch ( std::exception e ) {
        if (Config.is_set("--verbose") || Config.is_set("--vverbose"))
            std::cerr << "continuing with non-simplified formula " << f << std::endl;
        return f;
    }
}
    
}
