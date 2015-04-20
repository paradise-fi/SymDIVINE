#include "toolkit/hash.h"
#include "llvmsym/smtdatastore.h"

#include <unordered_map>
#include <set>
#include <unordered_set>
#include <vector>
#include <memory>

using namespace llvm_sym;

// reference counting blobs
struct Blob {
  int size;
  char *mem;
  size_t *refcount;

  size_t explicit_size;

  // Blob getExpl() const { return *this; }
  char* getSymb() const { return mem + explicit_size; }

  Blob( size_t s, size_t e_s ) : size( s ), mem( new char[ size ] ),
                                 refcount( new size_t( 1 ) ), explicit_size( e_s ) {}

  Blob( const Blob &snd ) : size( snd.size ), mem( snd.mem ),
                            refcount( snd.refcount ),
                            explicit_size( snd.explicit_size ) {
    ++*refcount;
  }

  Blob &operator=( const Blob snd ) {
    if ( this != &snd ) {
      assert( *refcount >= 1 );
      if ( --*refcount == 0 ) {
        delete [] mem;
      }
            
      mem = snd.mem;
      refcount = snd.refcount;
      explicit_size = snd.explicit_size;
      ++*refcount;
    }
    return *this;
  }

  ~Blob() {
    assert( *refcount >= 1 );
    if ( --*refcount == 0 ) {
      delete [] mem;
      delete refcount;
    }
  }

  /*  bool operator<( const Blob &snd ) const {
    if ( explicit_size != snd.explicit_size )
      return explicit_size < snd.explicit_size;
    return memcmp( mem, snd.mem, explicit_size ) < 0; // and data part is less than...
  }

  bool operator==( const Blob &snd ) const {
    if ( explicit_size != snd.explicit_size )
      return false;
    if ( memcmp( mem, snd.mem, explicit_size ) )
      return false;
    return SMTStore::equal( mem + explicit_size, snd.mem + explicit_size );
    }*/
};

struct blobHash {
  size_t operator()( const Blob &b ) const {
    hash128_t h = spookyHash( reinterpret_cast< void* >( b.mem ),
                              b.explicit_size, 0, 0 );
    return h.first ^ h.second;
  }
};

struct blobEqual {
  bool operator()( const Blob &b1, const Blob &b2 ) const {
    if ( b1.explicit_size != b2.explicit_size )
      return false;
    return memcmp( b1.mem, b2.mem, b1.explicit_size ) == 0;
  }
};

template< typename ExplState, typename SymbState,
          typename SymbContainer,
          typename ExplHasher, typename ExplEqual >
struct Database {
  std::unordered_map< ExplState, SymbContainer, ExplHasher, ExplEqual > table;
      
  bool seen( const ExplState &st ) {
    auto got = table.find( st );
    if ( got == table.end() )
      return false;
    else {
      SymbState sst;
      fillSym( sst, st );
      return got->second.seen( sst );
    }
  }

  //assume st is not yet stored
  void insert( const ExplState &st ) {
    auto got = table.find( st );
    if ( got == table.end() ) {
      SymbContainer sc;
      SymbState sst;
      fillSym( sst, st );
      //std::cout << "blobread: " << sst.me << std::endl;
      sc.insert( sst );
      table[ st ] = sc;
    }
    else {
      SymbState sst;
      fillSym( sst, st );
      got->second.insert( sst );
    }
  }

  bool insertCheck( const ExplState &st ) {
    auto got = table.find( st );
    if ( got == table.end() ) {
      SymbContainer sc;
      SymbState sst;
      fillSym( sst, st );
      sc.insert( sst );
      table[ st ] = sc;
      return true;
    }
    else {
      SymbState sst;
      fillSym( sst, st );
      return got->second.insertCheck( sst );
    }
  }

  void fillSym( SymbState &sst, const ExplState &st ) {
    const char * temp = st.getSymb();
    sst.readData( temp );
  }

  size_t size() {
    size_t retVal = 0;
    for ( auto e : table ) {
      retVal += e.second.size();
    }
    return retVal;
  }
};

struct EmptyValue {
  bool nothing;
  void readData( char *_ ) {}
};

struct EmptyCandidate {
  bool nothing;
  bool seen() { return false; }
  void insert() {}
  bool insertCheck() { return false; }
  size_t size() { return 0; }
};

struct ObliviousCandidate {
  bool nothing;
  bool seen() { return true; }
  void insert() {}
  bool insertCheck() { return true; }
  size_t size() { return 0; }
};

struct SMTEqual {
  bool operator()( const SMTStore &a, const SMTStore &b ) const {
    //assert( a.segments_mapping.size() == b.segments_mapping.size() );
    return a.subseteq( a, b ) && a.subseteq( b, a );
  }
};

struct SMTSubseteq {
  bool operator()( const SMTStore &a, const SMTStore &b ) const {
    //assert( a.segments_mapping.size() == b.segments_mapping.size() );
    return a.subseteq( a, b );
  }
};

template< typename State, typename Hit >
struct LinearCandidate {
  std::vector< State > data;
  Hit hit;

  bool seen( const State &st ) {
    for ( auto e : data ) {
      if ( hit( e, st ) )
        return true;
    }
    return false;
  }

  void insert( const State &st ) {
    data.push_back( st );
  }

  bool insertCheck( const State &st ) {
    for ( auto e : data ) {
      if ( hit( st, e ) )
        return false;
    }
    data.push_back( st );
    return true;
  }

  size_t size() {
    return data.size();
  }
};

//State implements operator<
template< typename State >
struct OrderedCandidate {
  std::set< State > data;

  bool seen( const State &st ) {
    return data.count( st ) != 0;
  }

  void insert( const State &st ) {
    data.insert( st );
  }

  bool insertCheck( const State &st ) {
    return data.insert( st ).second;
  }

  size_t size() {
    return data.size();
  }
};

template< typename State, typename Hasher, typename Equal >
struct HashedCandidate {
  std::unordered_set< State, Hasher, Equal > data;

  bool seen( const State &st ) {
    return data.count( st ) != 0;
  }

  void insert( const State &st ) {
    State temp = st;
    temp.keep = true;
    data.insert( temp );
  }

  bool insertCheck( const State &st ) {
    if ( data.find( st ) != data.end() )
      return false;
    insert( st );
    return true;
  }

  size_t size() {
    return data.size();
  }
};
