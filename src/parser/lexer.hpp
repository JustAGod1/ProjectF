#pragma once

#include <optional>
#include "parser.hpp" // this is needed for symbol_type
#include "parser/node_location.hpp" // this is needed for symbol_type
#include "utils/string_type.hpp"

    
class Scanner  {
public:
  const String &s;
  Scanner(String& s) : s(s) {
  }

	yy::parser::symbol_type next_token();
private:

	yy::parser::symbol_type do_next_token();
  bool comment();
  bool white_space();
  bool new_line();
  std::optional<yy::parser::symbol_type> quote();
  std::optional<yy::parser::symbol_type> lpar();
  std::optional<yy::parser::symbol_type> rpar();
  std::optional<yy::parser::symbol_type> b_true();
  std::optional<yy::parser::symbol_type> b_false();
  std::optional<yy::parser::symbol_type> b_null();
  std::optional<yy::parser::symbol_type> integral();
  std::optional<yy::parser::symbol_type> real();
  std::optional<yy::parser::symbol_type> identifier();
  std::optional<yy::parser::symbol_type> error();
  std::optional<yy::parser::symbol_type> eof();

  NodeLocation loc;

  int current_idx = 0;

  std::optional<Char> peek_char();
  void consume_char();

  bool match_string(const StringView pat);
  bool digits(String& targ);
  bool maybe_minus(String& targ);

  int lexer_error(const char* msg) {
      std::cerr << "Error(" << loc <<  "): " << msg << std::endl;
      return 0;
  }

  std::vector<std::function<bool(Scanner*)>> ws_funcitons{
    &Scanner::comment,
    &Scanner::white_space,
    &Scanner::new_line,
  };
  std::vector<std::function<std::optional<yy::parser::symbol_type>(Scanner*)>> tok_funcitons{
    &Scanner::quote,
    &Scanner::lpar,
    &Scanner::rpar,
    &Scanner::b_true,
    &Scanner::b_false,
    &Scanner::b_null,
    &Scanner::integral,
    &Scanner::real,
    &Scanner::identifier,
    &Scanner::eof,
    &Scanner::error,
  };

};

