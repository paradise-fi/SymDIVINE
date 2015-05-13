%{
    #include "../llvmsym/Formula/rpn.h"
    #include <stdexcept>
    #include <memory>
    #include <vector>
    #include "ltl_parser.hpp"
    #include "ltl_tokens.h"

    using namespace llvm_sym;

    std::vector<std::shared_ptr<Formula>> formulas;
    std::string res_formula;

    void yyerror(const char *s) { throw std::runtime_error("ERROR: " + std::string(s)); }

    Formula* binary_op(int o, void* f1, void* f2) {
      Formula* a = (Formula*)f1;
      Formula* b = (Formula*)f2;
      Formula* res;
      switch(o) {
        case TPLUS:
          res = new Formula(*a + *b);
          break;
        case TMINUS:
          res = new Formula(*a - *b);
          break;
        case TMUL:
          res = new Formula(*a * *b);
          break;
        case TDIV:
          res = new Formula(*a / *b);
          break;
        case TRSHIFT:
          res = new Formula(*a >> *b);
          break;
        case TLSHIFT:
          res = new Formula(*a << *b);
          break;
        case TEQUAL:
          res = new Formula(*a == *b);
          break;
        case TCNE:
          res = new Formula(*a != *b);
          break;
        case TCLT:
          res = new Formula(*a < *b);
          break;
        case TCLE:
          res = new Formula(*a <= *b);
          break;
        case TCGT:
          res = new Formula(*a > *b);
          break;
        case TCGE:
          res = new Formula(*a >= *b);
          break;
        case TAND:
          res = new Formula(*a && *b);
          break;
        case TOR:
          res = new Formula(*a || *b);
          break;
        case TIMP:
          res = new Formula(!(*a) || *b);
          break;
        default:
          assert(false);
      }
      delete a;
      delete b;
      return res;
    }
%}

%union {
    void* formula;
    std::string* string;
    int token;
}

%error-verbose

%token <string> TIDENTIFIER TINTEGER TDOUBLE
%token <token> TCEQ TCNE TCLT TCLE TCGT TCGE TEQUAL
%token <token> TLPAREN TRPAREN TLBRACE TRBRACE TCOMMA TDOT
%token <token> TPLUS TMINUS TMUL TDIV TSEG TOFF TUND TAND TOR TIMP
%token <token> TNEG TRSHIFT TLSHIFT TFUT TGLOB TUNTIL TREL TWEAK
%token <token> TSLPAREN TSRPAREN

%type <formula> formula arexpr ident arterm arfactor lexpr lterm ltlformula lnterm
%type <token> binary_ar_oper_low binary_ar_oper_high binary_la_oper binary_ll_oper
%type <string> ltl

%left TPLUS TMINUS
%left TMUL TDIV
%left TAND TOR TIMP
%left TRSHIFT TLSHIFT
%left TEQUAL TCNE TCGE TCGT TCLE TCLT
%left TUNTIL TWEAK TFUT TGLOB TREL TNEG

%start ltlformula

%%

ltlformula
  : ltl   { res_formula = *$1; delete $1; }

formula
  : lexpr  { formulas.push_back(std::shared_ptr<Formula>((Formula*)$1)); }
  ;

ident
  : TSEG TINTEGER TUND TOFF TINTEGER { $$ = new Formula(Formula::buildIdentifier(Formula::Ident(atoi($2->c_str()), atoi($5->c_str()), 42, 42)));}
  ;

binary_ar_oper_low
  : TPLUS   { $$ = $1; }
  | TMINUS  { $$ = $1; }
  | TRSHIFT { $$ = $1; }
  | TLSHIFT { $$ = $1; }
  ;

binary_ar_oper_high
  : TMUL    { $$ = $1; }
  | TDIV    { $$ = $1; }
  ;

arexpr
  : arterm                           { $$ = $1; }
  | arexpr binary_ar_oper_low arterm { $$ = binary_op($2, $1, $3); }
  ;

arfactor
  : ident                       { $$ = $1; }
  | TINTEGER TLPAREN TINTEGER TRPAREN { $$ = new Formula(Formula::buildConstant(atoi($1->c_str()), atoi($3->c_str()))); }
  | TINTEGER                    { $$ = new Formula(Formula::buildConstant(atoi($1->c_str()), 42)); }
  | TLPAREN arexpr TRPAREN      { $$ = $2; }
  ;   

arterm
  : arfactor  { $$ = $1; }
  | arterm binary_ar_oper_high arfactor { $$ = binary_op($2, $1, $3); }
  ;

binary_la_oper
  : TEQUAL  { $$ = $1; }
  | TCNE    { $$ = $1; }
  | TCLT    { $$ = $1; }
  | TCLE    { $$ = $1; }
  | TCGT    { $$ = $1; }
  | TCGE    { $$ = $1; }
  ;

binary_ll_oper
  : TAND    { $$ = $1; }
  | TOR     { $$ = $1; }
  | TIMP    { $$ = $1; }
  ;

lterm
  : TLPAREN lexpr TRPAREN         { $$ = $2; }
  | arexpr binary_la_oper arexpr  { $$ = binary_op($2, $1, $3); }
  ;

lnterm
  : lterm      { $$ = $1; }
  | TNEG lnterm { $$ = new Formula(!*((Formula*)$2)); delete (Formula*)$2; }
  ;

lexpr
  : lterm                         { $$ = $1; }
  | lexpr binary_ll_oper lnterm    { $$ = binary_op($2, $1, $3); }
  ;

ltl
  : TSLPAREN formula TSRPAREN { $$ = new std::string("ap" + std::to_string(formulas.size())); }
  | TFUT ltl              { $$ = new std::string("(F " + *$2 + ")"); delete $2; }
  | TGLOB ltl             { $$ = new std::string("(G " + *$2 + ")"); delete $2; }
  | ltl TUNTIL ltl        { $$ = new std::string("(" + *$1 + " U " + *$3 + ")"); delete $1; delete $3; }
  | ltl TREL ltl          { $$ = new std::string("(" + *$1 + " R " + *$3 + ")"); delete $1; delete $3; }
  | ltl TWEAK ltl         { $$ = new std::string("(" + *$1 + " W " + *$3 + ")"); delete $1; delete $3; }
  | TNEG ltl              { $$ = new std::string("! " + *$2); delete $2; }
  | ltl TAND ltl          { $$ = new std::string(*$1 + " && " + *$3); delete $1; delete $3; }
  | ltl TOR ltl           { $$ = new std::string(*$1 + " || " + *$3); delete $1; delete $3; }
  | ltl TIMP ltl          { $$ = new std::string("(" + *$1 + ") => (" + *$3 + ")"); delete $1; delete $3;}
  | TLPAREN ltl TRPAREN   { $$ = new std::string("(" + *$2 + ")"); delete $2; }
  ;

%%