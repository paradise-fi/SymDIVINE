#pragma once

#include <string>
#include <vector>
#include <cassert>
#include <ostream>
#include <sstream>
#include <tuple>
#include <z3++.h>
#include <map>

namespace llvm_sym {

struct Formula {
    struct Ident {
        short unsigned seg;
        short unsigned off;
        short unsigned gen;
        unsigned char bw;

        operator std::tuple< short unsigned, short unsigned, short unsigned, unsigned char >() {
            return std::make_tuple( seg, off, gen, bw );
        }
        bool operator<( const Ident &snd ) const {
            return std::tie( seg, off, gen, bw )
                < std::tie( snd.seg, snd.off, snd.gen, snd.bw );
        }
        bool operator==( const Ident &snd ) const {
            return seg == snd.seg
                && off == snd.off
                && gen == snd.gen
                && bw  == snd.bw;
        }

        bool operator!=( const Ident &snd ) const {
            return !( *this == snd );
        }

        Ident( short unsigned s, short unsigned o, short unsigned g, unsigned char b )
            : seg( s ), off( o ), gen( g ), bw( b ) {}
        Ident() = default;
    };
    struct Item {

        enum Kind {
            Op,
            Constant,
            Identifier,
            BoolVal
        };

        enum Operator {
            Plus,
            Minus,
            Times,
            Div,
            SRem,
            URem,
            Eq,
            NEq,
            LT,
            ULT,
            LEq,
            ULEq,
            GT,
            UGT,
            GEq,
            UGEq,
            And,
            Xor,
            Or,
            BOr,
            BAnd,
            BNot,
            Not,
            Shl,
            Concat,
            Shr,
            SExt,
            ZExt,
            Trunc
        };

        Kind kind;
        Operator op;
        Ident id; // for Identifiers
        int value; // for Constants

        bool is_unary_op() const
        {
            assert( kind == Kind::Op );

            return op == SExt || op == ZExt || op == Trunc || op == Not || op == BNot;
        }

        bool dependsOn( short unsigned seg, short unsigned off, short unsigned gen ) const
        {
            return kind == Item::Kind::Identifier
                        && id.seg == seg
                        && id.off == off
                        && id.gen == gen;
        }

        bool operator==( const Item &snd ) const
        {
            if ( kind != snd.kind )
                return false;
            if ( kind == Kind::Identifier )
                return id == snd.id;
            if ( kind == Kind::Op )
                return op == snd.op;
            else
                return value == snd.value;
        }

        std::string toString() const
        {
            std::stringstream ss;
            switch ( kind ) {
                case Op:
                    switch ( op ) {
                        case Plus:  return " + ";
                        case Minus: return " - ";
                        case Times: return " * ";
                        case Div:   return " / ";
                        case Eq:    return " == ";
                        case SRem:    return " % ";
                        case URem:    return " % ";
                        case NEq:   return " != ";
                        case LT:    return " < ";
                        case ULT:    return " < ";
                        case LEq:   return " <= ";
                        case ULEq:   return " <= ";
                        case GT:    return " > ";
                        case UGT:   return " > ";
                        case GEq:   return " >= ";
                        case UGEq:   return " >= ";
                        case BAnd:   return " & ";
                        case Shl:   return " << ";
                        case Concat:   return " . ";
                        case Shr:   return " >> ";
                        case And:   return " && ";
                        case Xor:   return " ^ ";
                        case Or:    return " || ";
                        case BOr:    return " | ";
                        case Not:   return "!";
                        case BNot:   return "!";
                        case SExt:   return "SignExt ";
                        case ZExt:   return "ZeroExt ";
                        case Trunc:   return "Trunc ";
                    
                    }
                case Constant:
                    ss << value << "[" << (int)id.bw << "]";
                    return ss.str();
                    
                case Identifier:
                    ss << 'a'
                       << "_seg"
                       << id.seg
                       << "_off"
                       << id.off
                       << "_gen"
                       << id.gen
                       << "[" << (int)id.bw << "]";
                    return ss.str();
                case BoolVal:
                    if ( value )
                        return "true";
                    else
                        return "false";
            }
            assert( false );
            return "";
        }

      std::string print( std::map< Ident, unsigned > &m ) const {
        std::stringstream ss;
        switch ( kind ) {
        case Op:
          switch ( op ) {
          case Plus:  return " + ";
          case Minus: return " - ";
          case Times: return " * ";
          case Div:   return " / ";
          case Eq:    return " = ";
          case NEq:   return " != ";
          case SRem:   return " % ";
          case URem:   return " % ";
          case LT:    return " < ";
          case ULT:    return " < ";
          case LEq:   return " <= ";
          case ULEq:   return " <= ";
          case GT:    return " > ";
          case UGT:    return " > ";
          case GEq:   return " >= ";
          case UGEq:   return " >= ";
          case BAnd:   return " & ";
          case Shl:   return " << ";
          case Concat:return " . ";
          case Shr:   return " >> ";
          case And:   return " && ";
          case Or:    return " || ";
          case BOr:    return " | ";
          case Xor:    return " ^ ";
          case Not:   return "!";
          case BNot:   return "!";
          case Trunc:   return "Trunc ";
          case SExt:   return "SignExt ";
          case ZExt:   return "ZeroExt ";
                    
          }
        case Constant:
          ss << "0d" << (int)id.bw << "_" << value;
          return ss.str();
                    
        case Identifier:
          if ( id.gen == 0 )
            ss << "r" << m[id];
          else
            ss << "[seg" << id.seg << "_off" << id.off << "]";
          return ss.str();
        case BoolVal:
          if ( value )
            return "TRUE";
          else
            return "FALSE";
        }
        assert( false );
        return "";
      }

      bool operator<( const Item &i ) const {
        return std::tie( kind, op, id, value ) < std::tie( i.kind, i.op, i.id, i.value );
      }
    };

    std::vector< Item > _rpn;

    bool sane() const
    {
        if ( size() == 0 )
            return true;

        int stack_size = 0;
        for ( unsigned i = 0; i < size(); ++i ) {
            switch ( at( i ).kind ) {
                case Item::Constant:
                case Item::Identifier:
                case Item::BoolVal:
                    ++stack_size;
                    break;
                case Item::Op:
                    if ( !at( i ).is_unary_op() )
                        --stack_size;
                    break;
            }
        }
        return stack_size == 1;
    }

    void push( Item a )
    {
        _rpn.push_back( a );
    }

    Item &top() {
        return _rpn.back();
    }
   
    size_t size() const {
        return _rpn.size();
    }

    const Item &top() const {
        return _rpn.back();
    }
    
    Item &at( int i ) {
        return _rpn[ i ];
    }
    
    const Item &at( int i ) const {
        return _rpn[ i ];
    }

    bool dependsOn( int seg, int off, int gen ) const
    {
        for ( const Item &i : _rpn )
            if ( i.dependsOn( seg, off, gen ) )
                return true;
        return false;
    }

    void append( const Formula &b )
    {
        int prev_size = _rpn.size();
        (void) prev_size;
        assert( _rpn.size() + b._rpn.size() <= _rpn.capacity() );
        std::copy( b._rpn.begin(), b._rpn.end(), back_inserter( _rpn ) );
        assert( prev_size + b._rpn.size() == _rpn.size() );
    }

    Formula substitute( const Ident pattern, const Formula definition ) const
    {
        Formula result;
        assert( definition.sane() );

        std::vector< int > pattern_occurances;

        for ( unsigned i = 0; i < _rpn.size(); ++i ) {
            const auto &f = _rpn[ i ];
            if ( f.kind == Item::Identifier && f.id == pattern )
                pattern_occurances.push_back( i );
        }

        if ( pattern_occurances.empty() ) {
            assert( sane() );
            return *this;
        }

        int first = pattern_occurances[0];
        int last  = pattern_occurances.back();
        std::copy( _rpn.begin(), _rpn.begin() + first, back_inserter( result._rpn ) );
        std::copy( definition._rpn.begin(), definition._rpn.end(), back_inserter( result._rpn ) );
        for ( unsigned i = 0; i < pattern_occurances.size() - 1; ++i ) {
            int pos = pattern_occurances[i];
            int pos_next = pattern_occurances[i+1];
            std::copy( _rpn.begin() + pos + 1, _rpn.begin() + pos_next, back_inserter( result._rpn ) );
            std::copy( definition._rpn.begin(), definition._rpn.end(), back_inserter( result._rpn ) );
        }
        std::copy( _rpn.begin() + last + 1, _rpn.end(), back_inserter( result._rpn ) );

        assert( result.sane() );
        assert( !result._rpn.empty() );
        return result;
    }

    Formula substitute( Formula definition ) const
    {
        assert( definition.top().kind == Item::Op );
        assert( definition.top().op == Item::Eq );
        assert( definition.at( 0 ).kind == Item::Identifier );
        assert( definition.sane() );

        Ident id = definition.at(0).id;

        assert( definition._rpn.back().op == Item::Eq );
        definition._rpn.pop_back(); // ==
        assert( definition._rpn.front().kind == Item::Identifier );
        definition._rpn.erase( definition._rpn.begin() ); // identifier
        return substitute( id, definition );
    }

    static Formula joinBinary( const Formula &l, const Formula &r, Item::Operator op )
    {
        if ( op == Item::And && l.size() == 0 )
            return r;
        if ( op == Item::And && r.size() == 0 )
            return l;
        Formula result;

        result._rpn.reserve( l._rpn.size() + r._rpn.size() + 1 );
        result.append( l );
        result.append( r );

        Item i;
        i.kind = Item::Op;
        i.op = op;

        result.push( i );
        // std::cout << "formula: " << l << " " << r << std::endl;
        assert( result.sane() );
        assert( result._rpn.size() == 1 + l._rpn.size() + r._rpn.size() );
        return result;
    }
    
    static Formula joinUnary( const Formula &l, Item::Operator op, int value = 0 )
    {
        Formula result;

        result._rpn.reserve( l._rpn.size() + 1 );
        result.append( l );

        Item i;
        i.kind = Item::Op;
        i.op = op;
        i.value = value;

        result.push( i );
        assert( result.sane() );
        assert( result._rpn.size() == 1 + l._rpn.size() );
        return result;
    }

    public:

    Formula() = default;

    static Formula buildBoolVal( bool val )
    {
        Formula ret;
        Item i = { Item::BoolVal };
        i.kind = Item::BoolVal;
        i.value = val;
        ret.push( i );
        return ret;
    }

  static Formula buildConstant( int val, int bw )
    {
        assert( bw > 0 );
        Formula ret;
        Item i = { Item::Constant };
        i.kind = Item::Constant;
        i.value = val;
        i.id = Ident( 0, 0, 0, bw );
        ret.push( i );
        return ret;
    }

    static Formula buildIdentifier( Ident id )
    {
        assert( id.bw > 0 ); //bitwidth
        Formula ret;
        Item i = { Item::Identifier };
        i.kind = Item::Identifier;
        i.id = id;
        ret.push( i );
        return ret;
    }

    Formula buildZExt( int bits ) const
    {
        return joinUnary( *this, Item::ZExt, bits );
    }
    
    Formula buildBNot() const
    {
        return joinUnary( *this, Item::BNot );
    }

    Formula buildSExt( int bits ) const
    {
        return joinUnary( *this, Item::SExt, bits );
    }

    Formula buildTrunc( int bits ) const
    {
        return joinUnary( *this, Item::Trunc, bits );
    }

    Formula operator==( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Eq);
    }

    Formula operator!=( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::NEq );
    }

    Formula operator<=( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::LEq);
    }

    Formula operator<( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::LT);
    }

    Formula operator>=( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::GEq);
    }

    Formula operator>( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::GT);
    }

    Formula operator+( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Plus );
    }

    Formula operator-( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Minus );
    }

    Formula operator*( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Times );
    }

    Formula operator/( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Div );
    }

    Formula operator&&( const Formula &b ) const
    {
        if ( b.size() == 0 )
            return *this;
        if ( size() == 0 )
            return b;

        return joinBinary( *this, b, Item::And );
    }

    Formula operator&( const Formula &b ) const
    {
        if ( b.size() == 0 )
            return *this;
        if ( size() == 0 )
            return b;

        return joinBinary( *this, b, Item::BAnd );
    }

    Formula buildConcat( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Concat );
    }

    Formula buildURem( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::URem );
    }

    Formula buildSRem( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::SRem );
    }

    Formula buildUGEq( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::UGEq );
    }

    Formula buildUGT( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::UGT );
    }
    Formula buildULEq( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::ULEq );
    }
    Formula buildULT( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::ULT );
    }

    Formula operator<<( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Shl);
    }

    Formula operator>>( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Shr);
    }

    Formula operator||( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Or);
    }

    Formula operator|( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::BOr);
    }

    Formula operator^( const Formula &b ) const
    {
        return joinBinary( *this, b, Item::Xor);
    }
    
    Formula operator!() const
    {
        return joinUnary( *this, Item::Not);
    }
   
    void collectVariables(  std::vector< Ident > &ret) const
    {
        for ( const auto &i : _rpn ) {
            if ( i.kind == Item::Kind::Identifier )
                ret.push_back( i.id );
        }
    }

    bool findIsomorphicVariableMapping(
            const Formula &snd,
            std::map< Ident, Ident > &mapping ) const
    {
        std::map< Ident, Ident > reverse_mapping;

        for ( const auto &m : mapping )
            reverse_mapping[ m.second ] = m.first;

        if ( _rpn.size() != snd._rpn.size() )
            return false;

        for ( size_t i = 0; i < _rpn.size(); ++i ) {
            if ( at(i).kind != snd.at(i).kind )
                return false;
            
            switch ( at(i).kind ) {
                case Item::Kind::Op:
                    if ( at(i).op != snd.at(i).op )
                        return false;
                    break;
                case Item::Kind::Constant:
                case Item::Kind::BoolVal:
                    if ( at(i).value != snd.at(i).value )
                        return false;
                    break;
                case Item::Kind::Identifier: {
                    auto f = mapping.find( at(i).id );
                    auto rf = reverse_mapping.find( snd.at(i).id );
                    if ( f != mapping.end() && f->second != snd.at(i).id )
                        return false;
                    if ( rf != reverse_mapping.end() && rf->second != at(i).id )
                        return false;

                    mapping[ at(i).id ] = snd.at(i).id;
                    reverse_mapping[ snd.at(i).id ] = at(i).id;
                }
            }
        }
        return true;
    }

    friend std::ostream &operator<<( std::ostream &o, const Formula &f );

    std::string print( std::map< Ident, unsigned > &m ) const {
      std::vector< std::string > stack;

      for ( unsigned pos = 0; pos < size(); ++pos ) {
        std::string l,r;
        
        switch ( at( pos ).kind ) {
        case Formula::Item::Op:
          if ( at( pos ).op == Formula::Item::Not ) {
            assert( stack.size() >= 1 );
            l = stack.back(); stack.pop_back();
            stack.push_back( at( pos ).print( m ) + l );
          } else { 
            assert( stack.size() >= 2 );
            r = stack.back(); stack.pop_back();
            l = stack.back(); stack.pop_back();
            if ( pos == size() - 1 )
              stack.push_back( l + at( pos ).print( m ) + r );
            else
              stack.push_back( '(' + l + at( pos ).print( m ) + r + ')' );
          }
          break;
        case Formula::Item::Constant:
        case Formula::Item::Identifier:
        case Formula::Item::BoolVal:
          stack.push_back( at( pos ).print( m ) );
          break;
        }
      }

      assert( stack.size() == 1 );
      return stack[ 0 ];
    }
};

struct Definition {
    Formula::Ident symbol;
    Formula def;

    Definition() = default;
    Definition( const Formula::Ident i, const Formula d ) : symbol( i ), def( d ) { assert( !d._rpn.empty() ); }

    Formula to_formula() const {
        return Formula::buildIdentifier( symbol ) == def;
    }

    bool dependsOn( int seg, int off, int gen ) const
    {
        bool symbol_depends = symbol.seg == seg && symbol.off == off && symbol.gen == gen;
        return def.dependsOn( seg, off, gen ) || symbol_depends;
    }

    const Formula::Ident &getIdent() const {
        return symbol;
    }

    const Formula &getDef() const {
        return def;
    }
    
    void collectVariables(std::vector<Formula::Ident>& vars) const {
        def.collectVariables(vars);
        vars.push_back(symbol);
    }

    bool isInSegment( int sid ) const {
        return symbol.seg == sid;
    }

    bool isOffset( int off ) const {
        return symbol.off == off;
    }

    bool isGeneration( int gen ) const {
        return symbol.gen == gen;
    }

    Definition substitute( const Formula::Ident pattern, const Formula definition ) const {
        assert( definition.sane() );
        assert( def.sane() );
        Formula new_def = getDef().substitute( pattern, definition );
        assert( new_def.sane() );
        return Definition( getIdent(), new_def );
    }

    bool operator==( const Definition &snd ) const {
        return symbol == snd.symbol && def._rpn == snd.def._rpn;
    }

    bool operator<( const Definition &snd ) const {
        return std::make_pair( symbol, def._rpn ) < std::make_pair( snd.symbol, snd.def._rpn );
    }
};

std::ostream &operator<<( std::ostream &o, const Formula &f );

}

