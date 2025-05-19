#pragma once
#include <string_view>
#include "parser/location.hh"

class NodeLocation : public yy::location {
public:
  size_t char_offset_begin = 0;
  size_t char_offset_end = 0;

  void columns(counter_type columns = 1) {
    yy::location::columns(columns);
    char_offset_end+=columns;
  }
  void step() {
    yy::location::step();

    char_offset_begin = char_offset_end;
  }

  void print_line_error(const std::string_view s, std::ostream& out) const;
};
