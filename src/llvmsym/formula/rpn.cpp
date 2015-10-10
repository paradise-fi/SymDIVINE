#include <llvmsym/formula/rpn.h>

#include <ostream>
#include <vector>
#include <string>

namespace llvm_sym {

std::ostream &operator<<( std::ostream &o, const Formula &f )
{
    if ( f.size() == 0 )
        return o;
    std::vector< std::string > stack;

    for ( unsigned pos = 0; pos < f.size(); ++pos ) {
        std::string l,r;
        
        switch ( f.at( pos ).kind ) {
            case Formula::Item::Op:
                if ( f.at( pos ).is_unary_op() ) {
                    assert( stack.size() >= 1 );
                    l = stack.back(); stack.pop_back();
                    stack.push_back( f.at( pos ).toString() + l );
                } else {
                    assert( stack.size() >= 2 );
                    r = stack.back(); stack.pop_back();
                    l = stack.back(); stack.pop_back();
                    if ( pos == f.size() - 1 )
                        stack.push_back( l + f.at( pos ).toString() + r );
                    else
                        stack.push_back( '(' + l + f.at( pos ).toString() + r + ')' );
                }
                break;
            case Formula::Item::Constant:
            case Formula::Item::Identifier:
            case Formula::Item::BoolVal:
                stack.push_back( f.at( pos ).toString() );
                break;
        }
    }

    assert( stack.size() == 1 );
    o << stack[ 0 ];

    return o;
}

}

