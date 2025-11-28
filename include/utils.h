#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace utl {
unsigned int get_next_codepoint(const std::string &s, size_t &i);
bool is_devanagari(unsigned int cp);
std::vector<std::string> split_by_devanagari(const std::string &input);
std::vector<std::string> split_by_newline(const std::string &input);
} // namespace utl
