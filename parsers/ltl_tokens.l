%{
#include <string>
#include "ltl_parser.hpp"


#define SAVE_TOKEN yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t) (yylval.token = t)
%}

%option noyywrap

%%

[ \t\n]                 ;
[0-9]+                  SAVE_TOKEN; return TINTEGER;
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