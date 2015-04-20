#pragma once

#include <llvmsym/datastore.h>
#include <vector>
#include <sstream>
#include <string>
#include <iostream>
#include <map>
#include "llvmsym/graph.h"

namespace llvm_sym {

std::string translate( DataStore::VariableId variable ) {
  std::stringstream retStream;
  retStream << "r<" << variable.segmentId << "><" << variable.offset << ">";
  return retStream.str();
}
  
struct KnowledgeBase {
  typedef DataStore::Value Vl;
  typedef DataStore::Value::Type Typ;
  typedef DataStore::VariableId Vid;
  typedef std::set< Vid, classcomp > CoI;
  std::map< Vid, CoI, classcomp > overall_influence, curr_influence;
  std::map< Vid, int, classcomp > bitWmap;
  SimpleEdge se;

  void add( Vl a, Vl b, Vl c ) {
    if ( b.type == Typ::Constant ) {
      add( a, c );
      return;
    }
    if ( c.type == Typ::Constant ) {
      add( a, b );
      return;
    }

    Vid av, bv, cv;
    av = a.variable; bv = b.variable; cv = c.variable;
    CoI a_coi = curr_influence[ bv ];
    a_coi.insert( bv );
    a_coi.insert( curr_influence[ cv ].begin(), curr_influence[ cv ].end() );
    a_coi.insert( cv );
    curr_influence[ av ] = a_coi;
    overall_influence[ av ].insert( a_coi.begin(), a_coi.end() );
  }

  void add( Vl a, Vl b ) {
    Vid av, bv;
    av = a.variable;
    CoI a_coi, b_coi;
    if ( b.type == Typ::Constant ) {
      curr_influence[ av ] = a_coi;
      return;
    }
    bv = b.variable;
    a_coi = curr_influence[ bv ];
    a_coi.insert( bv );
    curr_influence[ av ] = a_coi;
    overall_influence[ av ].insert( a_coi.begin(), a_coi.end() );
  }

  void clear( Vl v ) {
    CoI v_coi;
    curr_influence[ v.variable ] = v_coi;
  }

  void erase( unsigned i ) {
    CoI e_coi;
    for ( auto e : curr_influence )
      if ( e.first.segmentId == i )
        e.second = e_coi;
  }

  void print() {
    for ( auto e : overall_influence ) {
      std::cout << translate( e.first ) << ": {";
      for ( auto s : e.second ) {
        std::cout << translate( s ) << " ";
      }
      std::cout << "}" << std::endl;
    }
  }
};
  
struct EmptyStore : public DataStore {
  enum IAri_Op { PLUS, MINUS, TIMES, DIVIDE, MODULO };
  typedef KnowledgeBase* Kbp;
  Kbp repo;
  
  //constructors and destructors (more might be needed)
  EmptyStore() {}
  EmptyStore( const EmptyStore &e ) {}
  ~EmptyStore() {}

  virtual size_t getSize() const { return sizeof( Kbp ); }
  
  // writes data to memory, advances mem (!)
  virtual void writeData( char *&mem ) const {
    *reinterpret_cast< Kbp* >( mem ) = repo;
    mem += sizeof( Kbp );
  }
  // reads data from memory, advances mem (!)
  virtual void readData( const char * &mem ) {
    repo = *reinterpret_cast< const Kbp* >( mem );
    mem += sizeof( Kbp );
  }
  
  virtual void implement_add( Value r, Value a, Value b ) {
    ari( r, a, b, PLUS );
  }
  virtual void implement_mult( Value r, Value a, Value b ) {
    ari( r, a, b, TIMES );
  }
  virtual void implement_sub( Value r, Value a, Value b ) {
    ari( r, a, b, MINUS );
  }
  virtual void implement_div( Value r, Value a, Value b ) {
    ari( r, a, b, DIVIDE );
  }
  virtual void implement_rem( Value r, Value a, Value b ) {
    ari( r, a, b, MODULO );
  }
  virtual void implement_and( Value r, Value a, Value b ) {
    abort();
  }
  virtual void implement_or( Value r, Value a, Value b ) {
    abort();
  }
  virtual void implement_xor( Value r, Value a, Value b ) {
    abort();
  }
  virtual void implement_left_shift( Value r, Value a, Value b ) {
      abort();
  }
  virtual void implement_right_shift( Value r, Value a, Value b ) {
      abort();
  }

  virtual void implement_store( Value to, Value from ) {
    repo->se.v = Formula::buildIdentifier( Formula::Ident( to.variable.segmentId,
                                                 to.variable.offset,
                                                 0,
                                                 repo->bitWmap[ to.variable ] ) );
    if ( from.type == DataStore::Value::Type::Constant )
      repo->se.u = Formula::buildConstant( (unsigned) from.constant.value,
                                           repo->bitWmap[ to.variable ] );
    else
      repo->se.u = Formula::buildIdentifier(
        Formula::Ident( from.variable.segmentId,
                         from.variable.offset,
                         0,
                         repo->bitWmap[ from.variable ] )
      );
    // repo->se.u = Formula::buildIdentifier( Formula::Ident( from.variable.segmentId,
    //                                              from.variable.offset,
    //                                              0,
    //                                              repo->bitWmap[ from.variable ] ) );
    repo->se.g = Formula::buildBoolVal( true );
    
    repo->add( to, from );
    std::cout << translate( to ) << " := " << translate( from ) << std::endl;
  }

  virtual void prune( Value a, Value b, ICmp_Op op ) {
    repo->se.v._rpn.clear();
    Formula af, bf;
    if ( a.type == DataStore::Value::Type::Constant )
      af = Formula::buildConstant( (unsigned) a.constant.value,
                                   repo->bitWmap[ a.variable ] );
    else
      af = Formula::buildIdentifier(
        Formula::Ident( a.variable.segmentId,
                         a.variable.offset,
                         0,
                         repo->bitWmap[ a.variable ] )
      );
    if ( b.type == DataStore::Value::Type::Constant )
      bf = Formula::buildConstant( (unsigned) b.constant.value,
                                   repo->bitWmap[ a.variable ] );
    else
      bf = Formula::buildIdentifier(
        Formula::Ident( b.variable.segmentId,
                         b.variable.offset,
                         0,
                         repo->bitWmap[ b.variable ] )
      );
    // af = Formula::buildIdentifier( Formula::Ident( a.variable.segmentId,
    //                                      a.variable.offset,
    //                                      0,
    //                                      repo->bitWmap[ a.variable ] ) );
    // bf = Formula::buildIdentifier( Formula::Ident( b.variable.segmentId,
    //                                      b.variable.offset,
    //                                      0,
    //                                      repo->bitWmap[ b.variable ] ) );
    switch ( op ) {
    case ICmp_Op::EQ:
      repo->se.g = (af == bf);
      break;
    case ICmp_Op::NE:
      repo->se.g = (af != bf);
      break;
    case ICmp_Op::UGT:
      repo->se.g = (af > bf);
      break;
    case ICmp_Op::UGE:
      repo->se.g = (af >= bf);
      break;
    case ICmp_Op::ULT:
      repo->se.g = (af < bf);
      break;
    case ICmp_Op::ULE:
      repo->se.g = (af <= bf);
      break;
    case ICmp_Op::SGT:
      repo->se.g = (af > bf);
      break;
    case ICmp_Op::SGE:
      repo->se.g = (af >= bf);
      break;
    case ICmp_Op::SLT:
      repo->se.g = (af < bf);
      break;
    case ICmp_Op::SLE:
      repo->se.g = (af <= bf);
      break;
    }
    
    repo->add( a, b );
    std::cout << "assume( " << translate( a ) << " " << translate( op )
              << " " << translate( b ) << ")" << std::endl;
  }
  
  virtual void implement_input( Value id, unsigned bw ) {
    repo->se.v = Formula::buildIdentifier( Formula::Ident( id.variable.segmentId,
                                                 id.variable.offset,
                                                 0,
                                                 repo->bitWmap[ id.variable ] ) );
    repo->se.u = Formula::buildIdentifier( Formula::Ident( 0, 0, 1, bw ) );
    repo->se.g = Formula::buildBoolVal( true );
    repo->clear( id );
    std::cout << translate( id ) << " := i" << bw << std::endl;
  }

  bool empty() const {
    return false;
  }

  virtual void addSegment( unsigned id, const std::vector< int > &bws ) {
    std::cout << "Add variables: ";
    for ( unsigned i = 0; i < bws.size(); i++ ) {
      Value v;
      v.type = Value::Type::Variable;
      v.variable.segmentId = id;
      v.variable.offset = i;
      repo->bitWmap[ v.variable ] = bws.at( i );
      std::cout << translate( v ) << bws.at( i ) << " ";
    }
    std::cout << std::endl;
  }

  virtual void eraseSegment( int id ) {
    repo->erase( id );
    std::cout << "Removed segment: " << id << std::endl;
  }

  friend std::ostream & operator<<( std::ostream & o, const EmptyStore &v );
  
  virtual void clear() {}

private:

  void ari( Value r, Value a, Value b, IAri_Op op ) {
    std::cout << "build ari" << std::endl;
    repo->se.v = Formula::buildIdentifier(
      Formula::Ident( r.variable.segmentId,
                       r.variable.offset,
                       0,
                       repo->bitWmap[ r.variable ] )
    );
    Formula af, bf;
    if ( a.type == DataStore::Value::Type::Constant )
      af = Formula::buildConstant( (unsigned) a.constant.value,
                                   repo->bitWmap[ r.variable ] );
    else
      af = Formula::buildIdentifier(
        Formula::Ident( a.variable.segmentId,
                         a.variable.offset,
                         0,
                         repo->bitWmap[ a.variable ] )
      );
    if ( b.type == DataStore::Value::Type::Constant )
      bf = Formula::buildConstant( (unsigned) b.constant.value,
                                   repo->bitWmap[ r.variable ] );
    else
      bf = Formula::buildIdentifier(
        Formula::Ident( b.variable.segmentId,
                         b.variable.offset,
                         0,
                         repo->bitWmap[ b.variable ] )
      );
    repo->se.g = Formula::buildBoolVal( true );
    
    switch ( op ) {
    case PLUS :
      repo->se.u = af + bf;
      break;
    case MINUS :
      repo->se.u = af - bf;
      break;
    case TIMES :
      repo->se.u = af * bf;
      break;
    case DIVIDE :
      repo->se.u = af / bf;
      break;
    default :
      break;
    // case MODULO :
    //   repo->se.u = af + bf;      retStream << "%";
    //   break;
    }
    
    repo->add( r, a );
    repo->add( r, b );
    std::cout << translate( r ) << " := " << translate( a ) << " "
              << translate( op ) << " " << translate( b ) << std::endl;
  }

  std::string translate( Value v ) {
    std::stringstream retStream;
    if ( v.type == DataStore::Value::Type::Variable )
      retStream << "r<" << v.variable.segmentId << "><" << v.variable.offset << ">";
    else
      retStream << v.constant.value;
    return retStream.str();
  }

  std::string translate( IAri_Op op ) {
    std::stringstream retStream;
    switch ( op ) {
    case PLUS :
      retStream << "+";
      break;
    case MINUS :
      retStream << "-";
      break;
    case TIMES :
      retStream << "*";
      break;
    case DIVIDE :
      retStream << "/";
      break;
    case MODULO :
      retStream << "%";
      break;
    }
    return retStream.str();
  }

  std::string translate( ICmp_Op op ) {
    std::stringstream retVal;
    switch ( op ) {
    case ICmp_Op::EQ:
      retVal << "==";
      break;
    case ICmp_Op::NE:
      retVal << "!=";
      break;
    case ICmp_Op::UGT:
      retVal << ">";
      break;
    case ICmp_Op::UGE:
      retVal << ">=";
      break;
    case ICmp_Op::ULT:
      retVal << "<";
      break;
    case ICmp_Op::ULE:
      retVal << "<=";
      break;
    case ICmp_Op::SGT:
      retVal << ">";
      break;
    case ICmp_Op::SGE:
      retVal << ">=";
      break;
    case ICmp_Op::SLT:
      retVal << "<";
      break;
    case ICmp_Op::SLE:
      retVal << "<=";
      break;
    }
    return retVal.str();
  }
};

std::ostream & operator<<( std::ostream & o, const EmptyStore &v );
  
}
