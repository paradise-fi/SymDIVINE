%{
#include <string>
#include <parser.hpp>


#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}

%option noyywrap

%%

[ \t\n]                 ;
[0-9]+                  SAVE_TOKEN; return TINTEGER;
"["						return TOKEN(TSLPAREN);
"]"						return TOKEN(TSRPAREN);
"_"						return TOKEN(TUND);
"seg"					return TOKEN(TSEG);
"off"					return TOKEN(TOFF);
"="                     return TOKEN(TEQUAL);
"!="                    return TOKEN(TCNE);
"<"                     return TOKEN(TCLT);
"<="                    return TOKEN(TCLE);
">"                     return TOKEN(TCGT);
">="                    return TOKEN(TCGE);
"("                     return TOKEN(TLPAREN);
")"                     return TOKEN(TRPAREN);
"+"                     return TOKEN(TPLUS);
"-"                     return TOKEN(TMINUS);
"*"                     return TOKEN(TMUL);
"/"                     return TOKEN(TDIV);
">>"					return TOKEN(TRSHIFT);
"<<"					return TOKEN(TLSHIFT);
"&&"					return TOKEN(TAND);
"||"					return TOKEN(TOR);
"=>"					return TOKEN(TIMP);
"!"						return TOKEN(TNEG);
"F"						return TOKEN(TFUT);
"G"						return TOKEN(TGLOB);
"U"						return TOKEN(TUNTIL);
"R"						return TOKEN(TREL);
"W"						return TOKEN(TWEAK);
.                       printf("Unknown token!\n"); yyterminate();

%%

#include <iostream>
#include <utility>
#include <vector>
#include <string>
#include <llvmsym/formula/rpn.h>
#include <memory>

using namespace llvm_sym;

extern std::vector<std::shared_ptr<Formula>> formulas;
extern std::string res_formula;

std::pair<std::vector<std::shared_ptr<llvm_sym::Formula>>, std::string>
parse_ltl(const std::string& formula, bool verbose, bool vverbose)
{
	YY_BUFFER_STATE s = yy_scan_string(formula.c_str());
	yyparse();
	yy_delete_buffer(s);
    if (verbose || vverbose) {
		std::cout << "Input formula: " << formula << "\n";
		std::cout << "Parsed formula: " << res_formula << "\n";
		for (size_t i = 0; i != formulas.size(); i++) {
			std::cout << "ap" << i + 1 << ": " << *formulas[i] << "\n";
		}
	}
	return std::make_pair(formulas, res_formula);
}