#include "parser/lexer.hpp"
#include <cwctype>
#include <optional>
#include <string>
#include <codecvt>
#include "unicodelib.h"

yy::parser::symbol_type Scanner::next_token() {
  auto r = do_next_token();
  //std::cout << "tok: " << r.name() << std::endl;
  return r;
}

yy::parser::symbol_type Scanner::do_next_token() {
  while (true) {
    bool matched = false;
    for (auto& ws : this->ws_funcitons) {
      if (ws(this)) {
        matched = true;
      }
    }
    if (!matched) {
      break;
    }
  }
  loc.step();
  auto loc_before = loc;
  auto idx_before = current_idx;
  for (auto& p : this->tok_funcitons) {
    auto r = p(this);
    if (r.has_value()) {
      return *r;
    } else {
      loc = loc_before;
      current_idx = idx_before;
    }
  }

  assert_unverbose(false, "impossible");
}

std::optional<Char> Scanner::peek_char() {
  if (current_idx >= s.length()) {
    return std::nullopt;
  } 
  wchar_t c = (current_idx == s.length() - 1) ? 0 : s[current_idx];

  return c;
}

void Scanner::consume_char() {
  if (current_idx > s.length()) {
    return;
  } 
  wchar_t c = (current_idx == s.length()) ? 0 : s[current_idx];
  loc.columns();
  if (c == U'\n') {
    loc.lines();
  }

  current_idx++;
}

template<typename... T>
bool any_match(std::optional<wchar_t> v, T... variants) {
  if (!v.has_value()) {
    return false;
  }
  return (((v == variants) || ... || false));
}

bool Scanner::comment() {
  if (peek_char() != U'#') {
    return false;
  }
  consume_char();

  while (!any_match(peek_char(), U'\n', 0)) {
    consume_char();
  }

  return true;
}

bool Scanner::white_space() {
  bool r = false;

  while (any_match(peek_char(), U' ', U'\t')) {
    consume_char();
    r = true;
  }

  return r;
}
bool Scanner::new_line() {
  if (any_match(peek_char(), U'\n')) {
    consume_char();
    return true;
  }
  return false;
}
std::optional<yy::parser::symbol_type> Scanner::quote() {
  if (any_match(peek_char(), U'\'', U'`')) {
    consume_char();
    return yy::parser::make_QUOTE(loc);
  }

  return std::nullopt;
}
std::optional<yy::parser::symbol_type> Scanner::lpar() {
  if (any_match(peek_char(), U'(')) {
    consume_char();
    return yy::parser::make_LPAREN(loc);
  }

  return std::nullopt;
}
std::optional<yy::parser::symbol_type> Scanner::rpar() {
  if (any_match(peek_char(), U')')) {
    consume_char();
    return yy::parser::make_RPAREN(loc);
  }

  return std::nullopt;
}

bool Scanner::match_string(const StringView pat) {
  for (auto c: pat) {
    if (!any_match(peek_char(), c)) {
      return false;
    }
    consume_char();
  }

  return true;
}

std::optional<yy::parser::symbol_type> Scanner::b_true() {
  if (match_string(U"true")) {
    return yy::parser::make_TRUE(loc);
  }
  return std::nullopt;
}
std::optional<yy::parser::symbol_type> Scanner::b_false() {
  if (match_string(U"false")) {
    return yy::parser::make_FALSE(loc);
  }
  return std::nullopt;
}
std::optional<yy::parser::symbol_type> Scanner::b_null() {
  if (match_string(U"null")) {
    return yy::parser::make_NIL(loc);
  }
  return std::nullopt;
}


bool Scanner::maybe_minus(String& targ) {
  if (any_match(peek_char(), U'-')) {
    consume_char();
    targ.push_back(U'-');
    return true;
  }
  return false;
}

bool Scanner::digits(String& targ) {
  bool r = false;
  while (true) {
    auto ch_opt = peek_char();
    if (!ch_opt.has_value()) {
      break;
    }
    wchar_t ch = *ch_opt;
    if (std::iswdigit(ch)) {
      targ.push_back(ch);
      consume_char();
      r = true;
    } else {
      break;
    }
  }

  return r;
}

std::optional<yy::parser::symbol_type> Scanner::integral() {
  String buf;
  maybe_minus(buf);
  auto has_digits = digits(buf);
  if (!has_digits) {
    return std::nullopt;
  }
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;

  return yy::parser::make_INTEGER(std::stoi(converter.to_bytes(buf)), loc);
}
std::optional<yy::parser::symbol_type> Scanner::real() {
  String buf;
  maybe_minus(buf);
  auto has_digits_head = digits(buf);
  if (!has_digits_head) {
    return std::nullopt;
  }
  if (any_match(peek_char(), U'.')) {
    buf.push_back('.');
    consume_char();
  } else {
    return std::nullopt;
  }
  auto has_digits_tail = digits(buf);
  if (!has_digits_tail) {
    return std::nullopt;
  }

  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
  return yy::parser::make_REAL(std::stof(converter.to_bytes(buf)), loc);
}

static bool is_emoji(char32_t cp) {
    using namespace unicode;

    if (block(cp) == Block::MiscellaneousSymbolsAndPictographs ||
        block(cp) == Block::Emoticons ||
        block(cp) == Block::TransportAndMapSymbols ||
        block(cp) == Block::SupplementalSymbolsAndPictographs ||
        block(cp) == Block::SymbolsAndPictographsExtendedA) {
        return true;
    }
    return false;
}

static bool is_id_start(char32_t cp) {
  return unicode::is_id_start(cp) || is_emoji(cp);
}

static bool is_id_part(char32_t cp) {
  return unicode::is_id_continue(cp) || is_emoji(cp);
}

std::optional<yy::parser::symbol_type> Scanner::identifier() {
  auto begin_opt = peek_char();
  if (!begin_opt.has_value()) {
    return std::nullopt;
  }
  
  String buf;
  Char begin = *begin_opt;
  if (is_id_start(begin)) {
    buf.push_back(begin);
    consume_char();
  } else {
    return std::nullopt;
  }

  while (peek_char().has_value() && is_id_part(*peek_char())) {
    buf.push_back(*peek_char());
    consume_char();
  }

  return yy::parser::make_IDENTIFIER(make_nn_shared<Identifier>(loc, buf), loc);;
  return std::nullopt;
}

std::optional<yy::parser::symbol_type> Scanner::error() {
  auto ch_opt = peek_char();
  if (ch_opt.has_value()) {
    consume_char();
    return yy::parser::make_ERROR(String{*ch_opt}, loc);
  }
  return std::nullopt;
}


std::optional<yy::parser::symbol_type> Scanner::eof() {
  auto ch_opt = peek_char();
  if (*ch_opt == 0) {
    return yy::parser::make_END(loc);
  }
  return std::nullopt;
}
