#include "utils/string_type.hpp"

#include <codecvt>

std::string to_normal_string(const String& s) {
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
  return converter.to_bytes(s);
}
