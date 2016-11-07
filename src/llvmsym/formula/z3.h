#pragma once

#include <llvmsym/formula/rpn.h>

#include <z3++.h>
#include <toolkit/utils.h>
#include <vector>

namespace llvm_sym {

z3::expr forall( const std::vector< z3::expr > &v, const z3::expr & b );
z3::expr exists( const std::vector< z3::expr > &v, const z3::expr & b );

TriState is_const(z3::expr& expr);

z3::expr toz3( const Formula &f, char vpref, z3::context &c );

const Formula fromz3( const z3::expr &expr );

Formula simplify( const Formula &f );
Formula cheap_simplify( const Formula &f );
Formula simplify( const z3::expr f, std::string tactics );

}

