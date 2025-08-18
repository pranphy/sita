#include <string>
#include <vector>

#include "utils.h"


namespace utl{

// Function to check if a Unicode code point is Devanagari or ZWJ/ZWNJ
bool is_devanagari(unsigned int codepoint) {
    // Check for Devanagari Unicode block
    if (codepoint >= 0x0900 && codepoint <= 0x097F) {
        return true;
    }
    // Check for ZWJ and ZWNJ
    if (codepoint == 0x200D || codepoint == 0x200C) {
        return true;
    }
    return false;
}

// Simple UTF-8 decoder to get the next code point
unsigned int get_next_codepoint(const std::string& s, size_t& pos) {
    if (pos >= s.length()) {
        return 0;
    }
    unsigned int codepoint = 0;
    unsigned char first_byte = s[pos];

    if ((first_byte & 0x80) == 0) {
        codepoint = first_byte;
        pos += 1;
    } else if ((first_byte & 0xE0) == 0xC0) {
        codepoint = (first_byte & 0x1F) << 6;
        codepoint |= (s[pos + 1] & 0x3F);
        pos += 2;
    } else if ((first_byte & 0xF0) == 0xE0) {
        codepoint = (first_byte & 0x0F) << 12;
        codepoint |= (s[pos + 1] & 0x3F) << 6;
        codepoint |= (s[pos + 2] & 0x3F);
        pos += 3;
    } else if ((first_byte & 0xF8) == 0xF0) {
        codepoint = (first_byte & 0x07) << 18;
        codepoint |= (s[pos + 1] & 0x3F) << 12;
        codepoint |= (s[pos + 2] & 0x3F) << 6;
        codepoint |= (s[pos + 3] & 0x3F);
        pos += 4;
    } else {
        pos += 1;
    }
    return codepoint;
}

std::vector<std::string> split_by_devanagari(const std::string& input) {
    std::vector<std::string> result;
    if (input.empty()) {
        return result;
    }

    size_t pos = 0;
    size_t start_of_group = 0;
    unsigned int first_codepoint = get_next_codepoint(input, pos);
    bool in_devanagari_group = is_devanagari(first_codepoint);
    pos = 0; // Reset position for full iteration

    while (pos < input.length()) {
        size_t current_char_start_pos = pos;
        unsigned int codepoint = get_next_codepoint(input, pos);
        
        bool current_is_devanagari = is_devanagari(codepoint);
        
        if (current_is_devanagari != in_devanagari_group) {
            result.push_back(input.substr(start_of_group, current_char_start_pos - start_of_group));
            start_of_group = current_char_start_pos;
            in_devanagari_group = current_is_devanagari;
        }
    }
    
    if (start_of_group < input.length()) {
        result.push_back(input.substr(start_of_group));
    }
    
    return result;
}

}
std::vector<std::string> utl::split_by_newline(const std::string& input) {
    std::vector<std::string> result;
    size_t pos = 0;
    while (pos < input.length()) {
        size_t newline_pos = input.find('\n', pos);
        if (newline_pos == std::string::npos) {
            result.push_back(input.substr(pos));
            break;
        }
        result.push_back(input.substr(pos, newline_pos - pos));
        pos = newline_pos + 1;
    }
    return result;
}
