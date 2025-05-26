#include "parser/node_location.hpp"
#include <algorithm>
#include <string_view>
#include <vector>
#include <iostream>
#include <cctype>
#include <fmt/format.h>
#include <fmt/color.h>

void NodeLocation::print_line_error(const StringView s, std::ostream& out) const {
  std::vector<std::string> line_numbers;

  int max_prefix_len = 0;
  for (int i = begin.line; i <= end.line; i++) {
    line_numbers.emplace_back(fmt::format("{}", i));
    if (max_prefix_len < line_numbers.back().length()) {
      max_prefix_len = line_numbers.back().length();
    }
  }

  int min_left = 999999;
  int max_right = 0;
  
  int current_length = 0;

  int current_line = 0;
  int left = char_offset_begin + 1 - begin.column;
  int right = char_offset_end;
  while (right < s.size() && s[right] != '\n') right++;
  if (right > left && s[right] == '\n') right--;

  auto print_line_num = [&](int i) {
    std::cout << line_numbers[i];
    int len = line_numbers[i].length();
    if (len < max_prefix_len) {
      for (int i = 0; i < max_prefix_len - len; i++) {
        fmt::print(" ");
      }
    }
    fmt::print(": ");
  };
  print_line_num(current_line);
  for (int i = left; i <= right; i++) {
    Char v = s[i];
    if (char_offset_begin <= i && i < char_offset_end) {
      fmt::print(fmt::fg(fmt::color::red),
          "{}",
          to_normal_string(String{&v, 1}));
    } else {
      fmt::print("{}", to_normal_string(String{&v, 1}));
    }
    if (v == '\n') {
      print_line_num(++current_line);
      current_length = 0;
      continue;
    }
    current_length++;
    if (!std::isblank(v)) {
      min_left = std::min(min_left, current_length);
      max_right = std::max(max_right, current_length);
    }
  }

  std::cout << '\n';

  if (begin.line != end.line) {
    fmt::print("{}",
        std::string(min_left+3, ' '));

    fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::white),
        "{}",
        std::string(max_right - min_left + 1, '~'));
  } else {
    fmt::print("{}",
        std::string(begin.column+3-1, ' '));

    fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::white),
        "{}",
        std::string(end.column - begin.column, '~'));
  }
  out << std::endl;
}
