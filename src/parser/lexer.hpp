#pragma once

/**
 * Generated Flex class name is yyFlexLexer by default. If we want to use more flex-generated
 * classes we should name them differently. See scanner.l prefix option.
 * 
 * Unfortunately the implementation relies on this trick with redefining class name
 * with a preprocessor macro. See GNU Flex manual, "Generating C++ Scanners" section
 */
#include <istream>
#include <sstream>
#if ! defined(yyFlexLexerOnce)
#include <FlexLexer.h>
#endif

// Scanner method signature is defined by this macro. Original yylex() returns int.
// Sinice Bison 3 uses symbol_type, we must change returned type. We also rename it
// to something sane, since you cannot overload return type.
#undef YY_DECL
#define YY_DECL yy::parser::symbol_type Scanner::next_token ()

#include "parser.hpp" // this is needed for symbol_type
#include "parser/node_location.hpp" // this is needed for symbol_type


    
class Scanner : public yyFlexLexer {
private:
  std::istringstream stream;
public:
  const std::string& s;
  Scanner(std::string& s) : s(s), stream(s) {
    switch_streams(stream, std::cout);
  }
	virtual ~Scanner() = default;
	virtual yy::parser::symbol_type next_token();

private:
  NodeLocation loc;

  int lexer_error(const char* msg) {
      std::cerr << "Error(" << loc <<  "): " << msg << std::endl;
      return 0;
  }

};

