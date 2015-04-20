#include <llvmsym/bdddatastore.h>
#include <set>

namespace llvm_sym {

void BDDStore::input_transition( Vnf var ) {
  /*  for ( auto e : var ) {
    set_me( bdd_exist( get_me(), bdd_ithvar( e ) ) );
    }*/

  bdd varset = bdd_true();
  std::set< int > todos;
  std::vector< int > oldV, newV;

  for ( int i = 1; i < mgr->num_vars; i++ ) {
    if ( (i & 1) == 0 )
      continue;
    todos.insert( i );
    varset = bdd_apply( varset, bdd_ithvar( i ), bddop_and );
  }

  for ( auto e : var )
    todos.erase( e );

  bdd T = bdd_true();
  for ( auto e : todos ) {
    oldV.push_back( e + 1 );
    newV.push_back( e );
    bdd temp = bdd_apply( bdd_ithvar( e + 1 ), bdd_ithvar( e ), bddop_biimp );
    T = bdd_apply( T, temp, bddop_and );
  }

  std::cout << "transition number of nodes: " << bdd_nodecount( T ) << std::endl;    
  set_me( bdd_appex( get_me(), T, bddop_and, varset ) );
  
  bddPair* bP = bdd_newpair();
  bdd_setpairs( bP, oldV.data(), newV.data(), oldV.size() );
  set_me( bdd_replace( get_me(), bP ) );
  bdd_freepair( bP );
}
  
//(\exists X^B,Y^B.me\wedge pred\wedge<var:=val>)[x'/x]
//var.bit-width can be 0  
void BDDStore::transition( bdd &pred, Vnf var, BVec &val ) {
  set_me( bdd_apply( get_me(), pred, bddop_and ) );
  bdd varset = bdd_true();
  std::set< int > todos;
  std::vector< int > oldV, newV;
  
  for ( int i = 1; i < mgr->num_vars; i++ ) {
    if ( (i & 1) == 0 )
      continue;
    oldV.push_back( i + 1 );
    newV.push_back( i );
    todos.insert( i );
    varset = bdd_apply( varset, bdd_ithvar( i ), bddop_and );
  }
  
  bdd T = bdd_true();
  for ( unsigned i = 0; i < var.size(); i++ ) {
    int e = var.at( i );
    bdd temp = bdd_apply( bdd_ithvar( e + 1 ), val.at( i ), bddop_biimp );
    todos.erase( e );
    T = bdd_apply( T, temp, bddop_and );
  }
  for ( auto e : todos ) {
    bdd temp = bdd_apply( bdd_ithvar( e + 1 ), bdd_ithvar( e ), bddop_biimp );
    T = bdd_apply( T, temp, bddop_and );
  }
  
  std::cout << "transition number of nodes: " << bdd_nodecount( T ) << std::endl;    
  set_me( bdd_appex( get_me(), T, bddop_and, varset ) );
  
  bddPair* bP = bdd_newpair();
  bdd_setpairs( bP, oldV.data(), newV.data(), oldV.size() );
  set_me( bdd_replace( get_me(), bP ) );
  bdd_freepair( bP );
}
  
void BDDStore::ari( Value res, Value a, Value b, IAri_Op op ) {
  assert( res.type == Value::Type::Variable );
  BVec l, r, ress, rem, div;
  translate( l, a );
  translate( r, b );
  
  switch ( op ) {
  case PLUS : plus( ress, l, r ); break;
  case MINUS : minus( ress, l, r ); break;
  case TIMES : times_mod( ress, l, r ); break;
  case DIVIDE : divide_and_modulo( ress, rem, l, r ); break;
  case MODULO : divide_and_modulo( div, ress, l, r ); break;
  }

  Vnf vnf = mgr->naming[ res.variable ];
  bdd cond = bdd_true();
  transition( cond, vnf, ress );
}

void BDDStore::plus( BVec &res, BVec &l, BVec &r ) {
  bdd c = bdd_false();

  int bw = std::min( l.size(), r.size() );
  for ( int n = 0; n < bw; n++ ) {
    bdd tmp1, tmp2;

    tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_xor );
    tmp2 = bdd_apply( tmp1, c, bddop_xor );
    res.push_back( tmp2 );

    if ( n != bw - 1 ) {
      tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_or );
      tmp2 = bdd_apply( c, tmp1, bddop_and );
      tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_and );
      c = bdd_apply( tmp1, tmp2, bddop_or );
    }
  }
}

void BDDStore::times_mod( BVec &res, BVec &l, BVec &r ) {
  int min = std::min( l.size(), r.size() );
  res.resize( min, bdd_false() );
  BVec leftshift = l;
      
  for ( int n = 0; n < min; n++ ) {
    BVec added;
    plus( added, res, leftshift );
    for ( int m = 0; m < min; m++ )
      res.at( m ) = bdd_ite( r.at( n ), added.at( m ), res.at( m ) );

    leftshift.pop_back();
    leftshift.insert( leftshift.begin(), bdd_false() );
  }
}

void BDDStore::minus( BVec &res, BVec &l, BVec &r ) {
  bdd c = bdd_false();

  int bw = std::min( l.size(), r.size() );
  for ( int n = 0; n < bw; n++ ) {
    bdd tmp1, tmp2, tmp3;
    
    tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_xor );
    tmp2 = bdd_apply( tmp1, c, bddop_xor );
    res.push_back( tmp2 );

    if ( n != bw - 1 ) {
      tmp1 = bdd_apply( r.at( n ), c, bddop_or );
      tmp2 = bdd_apply( l.at( n ), tmp1, bddop_less );
      tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_and );
      tmp3 = bdd_apply( tmp1, c, bddop_and );
      c = bdd_apply( tmp3, tmp2, bddop_or );
    }
  }
}

void BDDStore::divide_and_modulo( BVec &res, BVec &rem, BVec &l, BVec &r ) {
  int max = l.size() + r.size();
  res.resize( r.size(), bdd_false() );
  rem = l;
  for ( unsigned i = 0; i < r.size(); i++ )
    rem.push_back( bdd_false() );
  BVec div;
  div.resize( r.size(), bdd_false() );
  for ( unsigned i = 0; i < l.size(); i++ )
    div.push_back( l.at( i ) );

  for ( unsigned n = 0; n < r.size() + 1; n++ ) {
    BVec divLteRem;
    lte( divLteRem, div, rem );
    BVec remSubDiv;
    minus( remSubDiv, rem, div );

    for ( int m = 0; m < max; m++ ) {
      rem.at( m ) = bdd_ite( divLteRem.at( 0 ),
                             remSubDiv.at( m ),
                             rem.at( m ) );
    }

    if ( n > 0 )
      res.at( r.size() - n ) = divLteRem.at( 0 );

    /* Shift 'div' one bit right */
    div.erase( div.begin() );
    div.push_back( bdd_false() );
  }

  rem.resize( r.size() );
}
    
  /*void BDDStore::store( Vnf vnf, BVec &what, Vnf arg1, Vnf arg2 ) {
  assert( what.size() >= vnf.first );
  //create T(a,a')
  bdd T = bdd_true();
  std::vector< int > oldV, newV;
  bdd varset = bdd_true();
  //change the LHS
  for ( int i = mgr->basis / 4, j = 0; j < vnf.first; i++, j++ ) {
    oldV.push_back( i );
    newV.push_back( vnf.second + j );
    varset = bdd_apply( varset, bdd_ithvar( vnf.second + j ), bddop_and );
    bdd temp = bdd_apply( what.at( j ), bdd_ithvar( i ), bddop_biimp );
    T = bdd_apply( T, temp, bddop_and );
    
  }
  //preserve the RHS
  for ( int i = mgr->basis / 2, j = 0; j < arg1.first; i++, j++ ) {
    if ( vnf.second == arg1.second )
      break;
    oldV.push_back( i );
    newV.push_back( arg1.second + j );
    varset = bdd_apply( varset, bdd_ithvar( arg1.second + j ), bddop_and );
    bdd temp = bdd_apply( bdd_ithvar( arg1.second + j ), bdd_ithvar( i ),
                          bddop_biimp );
    T = bdd_apply( T, temp, bddop_and );
  }
  for ( int i = (mgr->basis / 4) * 3, j = 0; j < arg2.first; i++, j++ ) {
    if ( vnf.second == arg2.second || arg1.second == arg2.second )
      break;
    oldV.push_back( i );
    newV.push_back( arg2.second + j );
    varset = bdd_apply( varset, bdd_ithvar( arg2.second + j ), bddop_and );
    bdd temp = bdd_apply( bdd_ithvar( arg2.second + j ), bdd_ithvar( i ),
                          bddop_biimp );
    T = bdd_apply( T, temp, bddop_and );
  }
  
  std::cout << "Transition:" << std::endl;
  bdd_printtable( T );
  std::cout << "==========" << std::endl;
  set_me( bdd_appex( get_me(), T, bddop_and, varset ) );
  std::cout << "After relop:" << std::endl;
  bdd_printtable( get_me() );
  std::cout << "==========" << std::endl;
  //me = me[X'/X]
  bddPair* bP = bdd_newpair();
  bdd_setpairs( bP, oldV.data(), newV.data(), oldV.size() );
  set_me( bdd_replace( get_me(), bP ) );
  bdd_freepair( bP );
  }*/
  
void BDDStore::translate( BVec &res, Value &in ) {
  if ( in.type == Value::Type::Variable ) {
    Vnf temp = mgr->naming[ in.variable ];
    for ( auto e : temp )
      res.push_back( bdd_ithvar( e ) );
  }
  else {
    for ( int k = 0; k < 64; k++ )
      res.push_back( (in.constant.value & (1<<k)) ? bdd_true() : bdd_false() );
  }
}

void BDDStore::compare( BVec &res, BVec &l, BVec &r, ICmp_Op op ) {
  int bw = std::min( r.size(), l.size() );
  assert( bw != 0 );
  l.resize( bw );
  r.resize( bw );
  switch ( op ) {
  case ICmp_Op::EQ : case ICmp_Op::NE :
    res.push_back( bdd_true() );
    for ( unsigned n = 0; n < l.size(); n++ ) {
      bdd tmp1, tmp2;
      tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_biimp );
      tmp2 = bdd_apply( tmp1, res.at( 0 ), bddop_and );
      res.at( 0 ) = tmp2;
    }
    if ( op == ICmp_Op::NE )
      res.at( 0 ) = bdd_not( res.at( 0 ) );
    break;
  case ICmp_Op::UGT : case ICmp_Op::UGE : case ICmp_Op::SGT : case ICmp_Op::SGE :
    res.push_back( (op == ICmp_Op::UGT || op == ICmp_Op::SGT) ?
                   bdd_true() : bdd_false() );
    less( res, l, r, (op == ICmp_Op::SGT || op == ICmp_Op::SGE) );
    // std::cout << "bdd for ugt:" << std::endl;
    // bdd_printtable( res.at( 0 ) );
    // std::cout << "**********" << std::endl;
    res.at( 0 ) = bdd_not( res.at( 0 ) );
    break;
  case ICmp_Op::ULT : case ICmp_Op::ULE : case ICmp_Op::SLT : case ICmp_Op::SLE :
    res.push_back( (op == ICmp_Op::ULT || op == ICmp_Op::SLT) ?
                   bdd_false() : bdd_true() );
    less( res, l, r, (op == ICmp_Op::SLT || op == ICmp_Op::SLE) );
    break;
  }
}

void BDDStore::eq( BVec &res, BVec &l, BVec &r ) {
  res.push_back( bdd_true() );
   
  for ( unsigned n = 0; n < l.size(); n++ ) {
    bdd tmp1, tmp2;
    tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_biimp );
    tmp2 = bdd_apply( tmp1, res.at( 0 ), bddop_and );
    res.at( 0 ) = tmp2;
  }
}

void BDDStore::less( BVec &res, BVec &l, BVec &r, bool sign ) {
  int bw = !sign ? l.size() : (l.size() - 1);
  //std::cout << "bw: " << bw << std::endl;
  for ( int n = 0; n < bw; n++ ) {      
    bdd tmp1 = bdd_apply( l.at( n ), r.at( n ), bddop_less );
    bdd tmp2 = bdd_apply( l.at( n ), r.at( n ), bddop_biimp );
    bdd tmp3 = bdd_apply( tmp2, res.at( 0 ), bddop_and );
    bdd tmp4 = bdd_apply( tmp1, tmp3, bddop_or );
    //std::cout << "partial comparison:" << std::endl;
    res.at( 0 ) = tmp4;
    //bdd_printtable( res.at( 0 ) );
    //std::cout << "&&&&&&&&&&" << std::endl;
  }
  if ( sign ) {
    bdd na = bdd_not( res.at( 0 ) );
    bdd nc = bdd_not( r.at( bw - 1 ) );
    bdd naConb = bdd_apply( na, l.at( bw - 1), bddop_and );
    bdd aConnc = bdd_apply( res.at( 0 ), nc, bddop_and );
    res.at( 0 ) = bdd_apply( naConb, aConnc, bddop_or );
  }
}

void BDDStore::lte( BVec &res, BVec &l, BVec &r ) {
  res.push_back( bdd_true() );
  less( res, l, r, false );
}

std::ostream & operator<<( std::ostream & o, const BDDStore &v ) {
  o << "data:\n";        
  return o;
}

}

