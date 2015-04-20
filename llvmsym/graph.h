#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include "llvmsym/blobing.h"
#include "llvmsym/blobutils.h"
#include "llvmsym/thread.h"
#include "llvmsym/formula/rpn.h"

struct ControlEqual {
  bool operator()( const Control &c1, const Control &c2 ) const {
    if ( c1.context != c2.context ||
         c1.previous_bb != c2.previous_bb )
      return false;
    else
      return true;
    if ( c1.getSize() != c2.getSize() )
      return false;
    
    // char *mem1 = new char[ c1.getSize() ];
    // char *tm1 = mem1;
    // c1.writeData( tm1 );
    // char *mem2 = new char[ c2.getSize() ];
    // char *tm2 = mem2;
    // c2.writeData( tm2 );
    // bool retVal = memcmp( mem1, mem2, c1.getSize() );
    // delete [] mem1;
    // delete [] mem2;
    // return retVal;
  }
};

struct ControlHash {
  size_t operator()( const Control &c ) const {
    char *mem = new char[ c.getSize() ];
    char *tm = mem;
    c.writeData( tm );

    hash128_t h = spookyHash( reinterpret_cast< void* >( mem ),
                              c.getSize(), 0, 0 );
    bool retVal = h.first ^ h.second;
    delete [] mem;
    return retVal;
  }
};

struct SimpleEdge {
  Formula g, v, u;//guard, variable, update

  // void writeData( char * &mem ) const
  // {
  //   blobWrite( mem, pc, context, previous_bb );
  // }

  // void readData( const char * &mem )
  // {
  //   pc.clear();
  //   context.clear();
  //   blobRead( mem, pc, context, previous_bb );

  //   assert( pc.size() == context.size() && pc.size() == previous_bb.size() );
  // }
};

struct SEdgeEqual {
  bool operator()( const SimpleEdge &se1, const SimpleEdge &se2 ) const {
    if ( se1.g._rpn != se2.g._rpn || se1.v._rpn != se2.v._rpn ||
         se1.u._rpn != se2.u._rpn )
      return false;
    else
      return true;
    
    // size_t rs1 = sizeof( se1 );
    // size_t rs2 = sizeof( se2 );
    // if ( rs1 != rs2 )
    //   return false;
    
    // char *mem1 = new char[ rs1 ];
    // char *tm1 = mem1;
    // blobWrite( tm1, se1.g._rpn );
    // blobWrite( tm1, se1.v._rpn );
    // blobWrite( tm1, se1.u._rpn );
    // //blobWrite( tm1, se1 );
    // //se1.writeData( tm1 );
    // char *mem2 = new char[ rs2 ];
    // char *tm2 = mem2;
    // blobWrite( tm2, se2.g._rpn );
    // blobWrite( tm2, se2.v._rpn );
    // blobWrite( tm2, se2.u._rpn );
    // //blobWrite( tm2, se2 );
    // //se2.writeData( tm2 );

    // bool retVal = memcmp( mem1, mem2, rs1 );
    // delete [] mem1;
    // delete [] mem2;
    // return retVal;
  }
};

struct SEdgeHash {
  size_t operator()( const SimpleEdge &se ) const {
    size_t rs = representation_size( se.g._rpn, se.v._rpn, se.u._rpn );
    char *mem = new char[ rs ];
    char *tm = mem;
    blobWrite( tm, se.g._rpn );
    blobWrite( tm, se.v._rpn );
    blobWrite( tm, se.u._rpn );
    //c.writeData( se.g._rpn, se.v._rpn, se.u._rpn );

    hash128_t h = spookyHash( reinterpret_cast< void* >( mem ),
                              rs, 0, 0 );
    bool retVal = h.first ^ h.second;
    delete [] mem;
    return retVal;
  }
};


template< typename T, typename THash, typename TEqual >
struct BiMap {
  std::unordered_map< T, unsigned, THash, TEqual > t2u;
  std::vector< T > u2t;

  std::pair< unsigned, bool > insert( const T& t ) {
    unsigned id = u2t.size(); //std::cout << "id:" << id << std::endl;
    std::pair< T, unsigned > p( t, id );
    auto suc = t2u.insert( p );
    // std::cout << "not found:" << (t2u.find( t ) == t2u.end()) << std::endl;
    std::pair< unsigned, bool > retVal;
    if ( suc.second ) {
      u2t.push_back( t );
      retVal.first = id;
      retVal.second = true;
    }
    else {
      retVal.first = suc.first->second;
      retVal.second = false;
    }
    return retVal;    
  }

  bool find( const T& t ) {
    return (t2u.find( t ) != t2u.end());
  }

  unsigned get( const T& t ) {
    auto it = t2u.find( t );
    if ( it == t2u.end() )
      return 0xFFFFFFFF;
    else
      return it->second;
  }

  T& get( unsigned u ) {
    return u2t.at( u );
  }

  unsigned size() {
    return u2t.size();
  }
};

struct NuSMVMap {
  struct VarUpdate {
    //Variable r;
    unsigned c;
    Formula g, u;
  };

  struct StateUpdate {
    unsigned c, u;
    Formula g;
  };

  //typedef std::vector< unsigned > Tuple;

  std::map< Formula::Ident, std::vector< VarUpdate > > updatesVars;
  std::vector< StateUpdate > updatesStates;

  void push( unsigned from, const SimpleEdge &e, unsigned to ) {
    VarUpdate vu;
    StateUpdate su;
    su.c = from;
    su.u = to;
    su.g = e.g;
    updatesStates.push_back( su );
    vu.c = from;
    vu.g = e.g;
    vu.u = e.u;
    if ( e.v.size() == 1 && e.v.at( 0 ).kind == Formula::Item::Identifier )
      updatesVars[ e.v.at( 0 ).id ].push_back( vu );
  }

  std::string print() {
    std::stringstream ss;
    ss << "MODULE main\nIVAR\n\tinput : unsigned word[64];\nVAR\n";

    std::map< int, std::vector< Formula::Ident > > varsBw;
    std::map< Formula::Ident, unsigned > varsId;
    unsigned id = 0;
    for ( auto e : updatesVars ) {
      varsBw[ e.first.bw ].push_back( e.first );
      varsId[ e.first ] = id++;
    }
    //printout variable declarations
    //ss << "\tinput : unsigned word[64];\n\t";
    for ( auto e : varsBw ) {
      //      ss << "\t";
      for ( unsigned u = 0; u < e.second.size(); u++ ) {
        ss << "\tr" << varsId[ e.second.at( u ) ];
        ss << " : unsigned word[" << e.first << "];\n";
      // if ( u != e.second.size() - 1 )
        //   ss << ", ";
      }

    }
    std::set< unsigned > states;
    for ( auto e : updatesStates ) {
      states.insert( e.c );
      states.insert( e.u );
    }
    //print state declaration
    ss << "\tstate : {deadlock, ";
    for ( auto it = states.begin(); it != states.end(); ) {
      ss << "s" << *it;
      if ( ++it != states.end() )
        ss << ", ";
    }
    //initial state
    ss << "};\nASSIGN\n\tinit( state ) := s" << updatesStates.at( 0 ).c << ";\n";
    //next expression for states
    ss << "\tnext( state ) := case\n";
    for ( auto e : updatesStates ) {
      ss << "\t\tstate = s" << e.c << " & " << e.g.print( varsId ) << " : s"
         << e.u << ";\n";
    }
    ss << "\t\tstate = deadlock : deadlock;\n\t\tTRUE : deadlock;\n\tesac;";
    //next expression for variables
    for ( auto e : updatesVars ) {
      ss << "\n\tnext( r" << varsId[ e.first ] << " ) := case\n";
      for ( auto u : e.second ) {
        ss << "\t\tstate = s" << u.c << " & " << u.g.print( varsId ) << " : "
           << u.u.print( varsId ) << ";\n";
      }
      ss << "\t\tTRUE : r" << varsId[ e.first ] << ";\n\tesac;";
    }
    return ss.str();
  }
};

template< typename State, typename StateHash, typename StateEquiv,
          typename Edge, typename EdgeHash, typename EdgeEquiv >
struct Graph {
  BiMap< State, StateHash, StateEquiv > states;
  BiMap< Edge, EdgeHash, EdgeEquiv > edges;

  std::vector< unsigned > csr_V;
  std::vector< std::pair< unsigned, unsigned > > csr_E;

  //temporary adjacency matrix
  std::unordered_map< State, std::unordered_map< Edge, State, EdgeHash, EdgeEquiv >,
                      StateHash, StateEquiv > temp_graph;
    
  void push_successor( const State &from, const Edge &by, const State &to ) {
    temp_graph[ from ][ by ] = to;
    states.insert( from );
    states.insert( to );
    edges.insert( by );
    // std::cout << "edge: if " << by.g << " " << by.v << ":=" << by.u << std::endl;
    // std::cout << "inserted: " << states.get( from ) << "-" << edges.get( by ) << "->"
    //           << states.get( to ) << std::endl;
  }

  void finalise() {
    // std::cout << "temp.size=" << temp_graph.size() << std::endl;
    csr_V.push_back( 0 );
    for ( unsigned u = 0; u < states.size(); u++ ) {
      auto e = temp_graph[ states.get( u ) ];
      csr_V.push_back( csr_V.back() + e.size() );
      for ( auto ee : e )
        csr_E.push_back( std::pair< unsigned, unsigned >( edges.get( ee.first ),
                                                          states.get( ee.second ) ) );
    }
    // std::cout << "size=" << csr_V.size() - 1 << std::endl;
    temp_graph.clear();
  }
  
  void get_successors( unsigned from, std::vector< unsigned > &bys,
                       std::vector< unsigned > &tos ) {
    for ( unsigned u = csr_V.at( from ); u < csr_V.at( from + 1 ); u++ ) {
      bys.push_back( csr_E.at( u ).first );
      tos.push_back( csr_E.at( u ).second );
    }
  }

  void translate( unsigned e_id, Edge &e ) {
    e = edges.get( e_id );
  }
};
