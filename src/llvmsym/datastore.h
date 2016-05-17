#pragma once

#include <vector>
#include <ostream>
#include <llvmsym/blobutils.h>
#include <llvmsym/llvmdata.h>

namespace llvm_sym {

enum class ICmp_Op {
    EQ = llvm::CmpInst::Predicate::ICMP_EQ,
    NE = llvm::CmpInst::Predicate::ICMP_NE,
    UGT = llvm::CmpInst::Predicate::ICMP_UGT,
    UGE = llvm::CmpInst::Predicate::ICMP_UGE,
    ULT = llvm::CmpInst::Predicate::ICMP_ULT,
    ULE = llvm::CmpInst::Predicate::ICMP_ULE,
    SGT = llvm::CmpInst::Predicate::ICMP_SGT,
    SGE = llvm::CmpInst::Predicate::ICMP_SGE,
    SLT = llvm::CmpInst::Predicate::ICMP_SLT,
    SLE = llvm::CmpInst::Predicate::ICMP_SLE
};

ICmp_Op icmp_negate( ICmp_Op what );

struct DataStore {
    struct VariableId {
        unsigned segmentId;
        unsigned offset;
    };

    struct Constant {
        uint64_t value;
        unsigned bw;
    };

    struct Value {
        enum class Type { Variable, Constant };

        Type type;
        bool pointer;

        union {
            VariableId variable;
            Constant constant;
        };

      Value( int64_t i, unsigned bw, bool is_ptr = false ) {
        type = Value::Type::Constant;
        constant.value = i;
        constant.bw = bw;
        pointer = is_ptr;
      }

      Value() {}
    };
    // returns size in bytes
    virtual size_t getSize() const = 0;

    // writes data to memory, advances mem (!)
    virtual void writeData( char *&mem ) const = 0;
    
    // reads data from memory, advances mem (!)
    virtual void readData( const char * &mem ) = 0;
    
    virtual void implement_add( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_mult( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_sub( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_div( Value result_id, Value a_id, Value b_id ) = 0;
    
    virtual void implement_urem( Value result_id, Value a_id, Value b_id ) = 0;
    
    virtual void implement_srem( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_and( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_or( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_xor( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_left_shift( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_right_shift( Value result_id, Value a_id, Value b_id ) = 0;

    virtual void implement_ZExt( Value result_id, Value a_id, int bw ) = 0;
    virtual void implement_SExt( Value result_id, Value a_id, int bw ) = 0;
    virtual void implement_Trunc( Value result_id, Value a_id, int bw ) = 0;

    virtual void implement_store( Value result_id, Value what ) = 0;

	virtual void implement_inttoptr(Value result_id, Value a_id) = 0;
    virtual void implement_ptrtoint(Value result_id, Value a_id) = 0;

    virtual void prune( Value a, Value b, ICmp_Op op ) = 0;

    // read _bw_ bits from input, assign to _input_variable_
    virtual void implement_input( Value input_variable, unsigned bw ) = 0;

    virtual bool empty() = 0;

    virtual void addSegment( unsigned id, const std::vector< int > &bit_widths ) = 0;

    virtual void eraseSegment( int id ) = 0;

  //   virtual static bool equal( char *mem_a, char *mem_b ) = 0;
  
  virtual void clear() = 0;
};

struct classcomp {
  bool operator() (const DataStore::VariableId& lhs,
                   const DataStore::VariableId& rhs) const {
    if ( lhs.segmentId < rhs.segmentId )
      return true;
    if ( lhs.segmentId == rhs.segmentId && lhs.offset < rhs.offset )
      return true;
    return false;
  }
};  

}

