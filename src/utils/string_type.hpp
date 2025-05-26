#pragma once
#include <string>
#include <string_view>
#include <fmt/format.h>
#include <fmt/format.h>

using String = std::u32string;
using StringView = std::u32string_view;
using Char = char32_t;


std::string to_normal_string(const String& s);
