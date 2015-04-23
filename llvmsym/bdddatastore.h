#pragma once

#include <llvmsym/datastore.h>
#include <vector>

#include "bdd.h"

namespace llvm_sym {

//manager structure for the global BDD
//mapping between program variables and Boolean variables is stored here
//global Boolean variables can only be added
//maximal number of input bits for any Left-hand side must be known in advance
//there probably is no reason to store even a pointer to this in blobs
struct SharedBDDRep {
  typedef std::vector< int > VarNfo;
  // typedef std::pair< unsigned, unsigned > VarName;
  // typedef std::map< VarName, VarNfo >::iterator Mit;
  
  int num_vars;
  int basis;
  std::map< DataStore::VariableId, VarNfo, classcomp > naming;
  std::vector< bdd > data;
  
  SharedBDDRep() : basis( 1 ) {
    bdd_init( 5000000, 50000 );
    bdd_setvarnum( basis ); //bottom $n$ variables are reserved for input
    bdd_setmaxincrease( 1000000 );
    //table = bdd_false();
    num_vars = basis;
  }

  void set_num_vars( int _num ) {
    num_vars += _num;
    bdd_setvarnum( num_vars );
  }

  //this destructor should be called only after all MyBdd and BDDSet instances
  //have been destroyed
  ~SharedBDDRep() {
    bdd_done();
  }
};

struct BDDStore : public DataStore {
  //inner types ---->  
  typedef std::vector< bdd > BVec;
  enum IAri_Op { PLUS, MINUS, TIMES, DIVIDE, MODULO };
  typedef SharedBDDRep::VarNfo Vnf;
  typedef SharedBDDRep* RepP;
  //<---- inner types

    //stored values
  RepP mgr;
  unsigned me;
  bool unseen;
  bool keep;

  //constructors and destructors (more might be needed)
  BDDStore() {
    unseen = true;
    keep = false;
    me = 0;
    //me = bdd_addref( bdd_false() );
  }

  BDDStore( const BDDStore &b ) {
    mgr = b.mgr;
    me = b.me;
    unseen = b.unseen;
    keep = b.keep;
  }

  ~BDDStore() {
    if ( !keep && !unseen )
      mgr->data.pop_back();
    //bdd_delref( me );
  }

  bdd get_me() const {
    //std::cout << "get: " << me << std::endl;
    assert( mgr->data.size() > me );
    return mgr->data.at( me );
  }

  void set_me( bdd _b ) {
    //print_table();
    if ( unseen ) {
      me = mgr->data.size();
      mgr->data.push_back( bdd() );
      //std::cout << "registered: " << me << std::endl;
      unseen = false;
    }
    mgr->data.at( me ) = _b;
    //print_table();
    //std::cout << "set: " << me << std::endl;
  }

  void print_table() {
    std::cout << "----------" << std::endl;
    for ( auto e : mgr->data ) {
      bdd_printtable( e );
    }
    std::cout << std::endl << "--------------------" << std::endl;
  }

  void inner_mapping() {
    //std::cout << "++++++++++" << std::endl;
    for ( auto e : mgr->naming ) {
      for ( auto i : e.second )
        std::cout << i << " -> "
                  << bdd_ithvar( i ).id() << std::endl;
    }
    std::cout << "++++++++++++++++++++" << std::endl;
  }

  virtual size_t getSize() const {
    return sizeof( RepP ) + sizeof( unsigned );
  }
  // writes data to memory, advances mem (!)
  virtual void writeData( char *&mem ) const {
    *reinterpret_cast< RepP* >( mem ) = mgr;
    mem += sizeof( RepP );
    //std::cout << "write: " << me << std::endl;
    *reinterpret_cast< unsigned* >( mem ) = me;
    mem += sizeof( unsigned );
  }
  // reads data from memory, advances mem (!)
  virtual void readData( const char * &mem ) {
    mgr = *reinterpret_cast< const RepP* >( mem );
    mem += sizeof( RepP );
    //std::cout << "read: " << me << std::endl;
    me = *reinterpret_cast< const unsigned* >( mem );
    mem += sizeof( unsigned );
  }
  
  virtual void implement_add( Value r, Value a, Value b ) {
    ari( r, a, b, PLUS );
  }
  virtual void implement_rem( Value r, Value a, Value b ) {
    ari( r, a, b, MODULO );
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
  virtual void implement_and( Value r, Value a, Value b ) {
      /*FIXME */
      abort();
  }
  virtual void implement_or( Value r, Value a, Value b ) {
      /*FIXME */
      abort();
  }
  virtual void implement_xor( Value r, Value a, Value b ) {
      /*FIXME */
      abort();
  }
  virtual void implement_left_shift( Value r, Value a, Value b ) {
      /*FIXME */
      abort();
  }
  virtual void implement_right_shift( Value r, Value a, Value b ) {
      /*FIXME */
      abort();
  }
  
  virtual void implement_store( Value to, Value from ) {
    assert( to.type == Value::Type::Variable );
    BVec bFrom;
    Vnf vnf = mgr->naming[ to.variable ];
    translate( bFrom, from );
    bdd cond = bdd_true();
    transition( cond, vnf, bFrom );
  }

  virtual void prune( Value a, Value b, ICmp_Op op ) {
    BVec bA, bB, res;
    translate( bA, a );
    translate( bB, b );
    compare( res, bA, bB, op );
    assert( res.size() == 1 );
    Vnf emptyVnf = Vnf( 0, 0 );
    BVec emptyBVec;
    transition( res.at( 0 ), emptyVnf, emptyBVec );
  }
  
  virtual void implement_input( Value id, unsigned bw ) {
    assert( id.type == Value::Type::Variable );
    Vnf vnf = mgr->naming[ id.variable ];
    assert( vnf.size() == bw );
    input_transition( vnf );
    // bdd cond = bdd_true();
    // BVec inputs;
    // for ( int i = 0; i < bw; i++ )
    //   inputs.push_back( bdd_ithvar( i ) );
    // transition( cond, vnf, inputs );
  }

  bool empty() const {
    return unseen || (get_me() == bdd_false());
  }

  virtual void addSegment( unsigned id, const std::vector< int > &bws ) {
    assert( !bws.empty() );
    VariableId tId;
    tId.segmentId = id;
    tId.offset = 0;
    if ( mgr->naming.find( tId ) != mgr->naming.end() )
      return;
    
    std::vector< Vnf > table;
    table.resize( bws.size() );
    bool on = true;
    int add_vars = 0;
    for ( int bw = 0; on; bw++ ) {
      on = false;
      for ( unsigned i = 0; i < bws.size(); i++ ) {
        if ( bws.at( i ) <= bw )
          continue;
        on = true;
        table.at( i ).push_back( mgr->num_vars + add_vars );
        add_vars += 2;
      }
    }

    for ( unsigned i = 0; i < table.size(); i++ ) {
      tId.offset = i;
      mgr->naming[ tId ] = table.at( i );
    }
    mgr->set_num_vars( add_vars );

    if ( unseen ) //TODO: fix
      set_me( bdd_true() );
  }

  //only locally erase the influence of those variables
  virtual void eraseSegment( int id ) {
    //???
    for ( auto e : mgr->naming ) {
      if ( static_cast<int>(e.first.segmentId) != id )
        continue;
      for ( auto i : e.second )
        set_me( bdd_exist( get_me(), bdd_ithvar( i ) ) );
    }
  }

  static bool equal( char *mem1, char *mem2 ) {
    BDDStore a, b;
    const char *a_readfrom = mem1;
    const char *b_readfrom = mem2;

    a.readData( a_readfrom );
    b.readData( b_readfrom );

    return a.mgr->data.at( a.me ) == b.mgr->data.at( b.me );
  }

  static size_t hash( char *mem1 ) {
    BDDStore a;
    const char *a_readfrom = mem1;

    a.readData( a_readfrom );

    return static_cast< size_t >( a.mgr->data.at( a.me ).id() );
  }

  friend std::ostream & operator<<( std::ostream & o, const BDDStore &v );
  
  virtual void clear() {}

  void implement_urem(Value result_id, Value a_id, Value b_id)
  {
      std::cout << "Unimplemented!";
      std::terminate();
  }

  void implement_srem(Value result_id, Value a_id, Value b_id)
  {
      std::cout << "Unimplemented!";
      std::terminate();
  }

  void implement_ZExt(Value result_id, Value a_id, int bw)
  {
      std::cout << "Unimplemented!";
      std::terminate();
  }

  void implement_SExt(Value result_id, Value a_id, int bw)
  {
      std::cout << "Unimplemented!";
      std::terminate();
  }

  void implement_Trunc(Value result_id, Value a_id, int bw)
  {
      std::cout << "Unimplemented!";
      std::terminate();
  }


private:

  void input_transition( Vnf );
  void transition( bdd&, Vnf, BVec& );
  void ari( Value, Value, Value, IAri_Op );
  void plus( BVec &, BVec &, BVec & );
  void times_mod( BVec &, BVec &, BVec & );
  void minus( BVec &, BVec &, BVec & );
  void divide_and_modulo( BVec &, BVec &, BVec &, BVec & );
  //  void store( Vnf, BVec &, Vnf, Vnf );
  void translate( BVec &, Value & );
  void compare( BVec &, BVec &, BVec &, ICmp_Op );
  void eq( BVec &, BVec &, BVec & );
  void less( BVec &, BVec &, BVec &, bool );
  void lte( BVec &, BVec &, BVec & );
};

struct BDDHash {
  size_t operator()( const BDDStore &bs ) const {
    return static_cast< size_t >( bs.mgr->data.at( bs.me ).id() );
  }
};

struct BDDEqual {
  bool operator()( const BDDStore &b1, const BDDStore &b2 ) const {
    return b1.mgr->data.at( b1.me ) == b2.mgr->data.at( b2.me );
  }
};
  
std::ostream & operator<<( std::ostream & o, const BDDStore &v );
  
}

