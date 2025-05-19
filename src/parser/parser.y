%language "C++"
%defines
%require "3.0"
%define parse.error verbose
%define api.token.constructor
%define api.value.type variant
%define parse.assert

%define api.location.type {NodeLocation}

//%locations

%code requires {
    #include <memory>
    #include <string>
    #include <vector>
    #include "parser/node_location.hpp" 
    #include "parser_structs.hpp"
    
    
    using ASTNodePtr = std::shared_ptr<ASTNode>;
    using ElementPtr = std::shared_ptr<Element>;
    using ListPtr = std::shared_ptr<List>;
    using AtomPtr = std::shared_ptr<Atom>;
    using LiteralPtr = std::shared_ptr<Literal>;
    using IdentifierPtr = std::shared_ptr<Identifier>;
    using QuotePtr = std::shared_ptr<Quote>;

  class Scanner;
}

%code top {
    #include <string>
    #include <iostream>
    #include "lexer.hpp"
    #include "parser.hpp"
namespace yy
  {
    
    static yy::parser::symbol_type yylex (Scanner& s) {
      return s.next_token();
    }
  }
#define YYLLOC_DEFAULT(Current, Rhs, N)                               \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).begin  = YYRHSLOC (Rhs, 1).begin;                   \
          (Current).char_offset_begin  = YYRHSLOC (Rhs, 1).char_offset_begin;                   \
          (Current).end    = YYRHSLOC (Rhs, N).end;                     \
          (Current).char_offset_end    = YYRHSLOC (Rhs, N).char_offset_end;                     \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).begin = (Current).end = YYRHSLOC (Rhs, 0).end;      \
          (Current).char_offset_begin = (Current).char_offset_end = YYRHSLOC (Rhs, 0).char_offset_end;      \
        }                                                               \
    while (false)

}


%param { Scanner &scanner }
%parse-param { std::shared_ptr<Program> program }

%locations

%token <std::string> ERROR "error token"
%token END 0 "end of file"
%token LPAREN "(" RPAREN ")"
%token QUOTE "'"
%token TRUE "true" FALSE "false"
%token <std::shared_ptr<Identifier>> IDENTIFIER
%token <int> INTEGER
%token <double> REAL

%type <std::shared_ptr<Program>> Program
%type <ElementPtr> Element
%type <ListPtr> List
%type <std::shared_ptr<Atom>> Atom
%type <QuotePtr> Quote
%type <std::shared_ptr<Literal>> Literal
%type <std::vector<InterpreterNodePtr>> ListInner

%start Program

%%

Program
    : Element {
        program->elements.push_back($1);
        $$ = program;
    }
    | Program Element {
        $1->elements.push_back($2);
        $$ = $1;
    }
    ;

ListInner 
    : ListInner Element {
      $1.push_back($2);
      $$ = std::move($1);
    }
    | Element {
      $$ = std::vector<InterpreterNodePtr>{$1};
    }
    ;

List
    : "(" ")" {
        auto list = std::make_shared<List>(@$, std::vector<InterpreterNodePtr>{});
        $$ = list;
    }
    | "(" ListInner ")" {
        $$ = std::make_shared<List>(@$, $2);
    }
    ;

Quote 
    : "'" Element {
      $$ = std::make_shared<Quote>(@$, $2);
    }

Element
    : Atom { $$ = $1; }
    | Literal { $$ = $1; }
    | List { $$ = $1; }
    | Quote { $$ = $1; }
    ;

Atom
    : IDENTIFIER {
        $$ = std::make_shared<Atom>(@$, std::move($1));
    }
    ;

Literal
    : INTEGER { $$ = makeLiteralInt(@$, $1); }
    | REAL { $$ = makeLiteralReal(@$, $1); }
    | "true" { $$ = makeLiteralBool(@$, true); }
    | "false" { $$ = makeLiteralBool(@$, false); }
    | "null" { $$ = makeLiteralNil(@$); }
    ;

%%

//std::shared_ptr<ListInner> g_program;

// Bison expects us to provide implementation - otherwise linker complains
void yy::parser::error(const location_type& loc, const std::string &message) {

        // Location should be initialized inside scanner action, but is not in this example.
        // Let's grab location directly from driver class.
	// cout << "Error: " << message << endl << "Location: " << loc << endl;

        std::cout << "Error: " << message << std::endl << "Error location: " << loc << std::endl;
        loc.print_line_error(scanner.s, std::cout);
}


