#include "terminal_parser.h"
#include "utils.h"
#include <iostream>
#include <sstream>

TerminalParser::TerminalParser() {
    // Initialize prompt patterns
    prompt_patterns = {
        std::regex(R"(\$ $)"),                    // Basic prompt
        std::regex(R"(# $)"),                     // Root prompt
        std::regex(R"(\w+@\w+:\S+[#$] $)"),       // user@host:path$ 
        std::regex(R"(\w+@\w+:\S+> $)"),          // user@host:path>
        std::regex(R"(\w+@\w+:\S+\) $)"),         // user@host:path) 
        std::regex(R"(\w+@\w+:\S+\[.*\]\$ $)"),   // user@host:path[git]$
        std::regex(R"(\w+@\w+:\S+\(.*\)\$ $)"),   // user@host:path(branch)$
        std::regex(R"(\w+@\w+:\S+\(.*\)\) $)"),   // user@host:path(branch))
        std::regex(R"(\w+@\w+:\S+\(.*\)> $)"),    // user@host:path(branch)>
        std::regex(R"(\w+@\w+:\S+\(.*\)\[.*\]\$ $)"), // Complex git prompt
    };
    
    // Initialize escape sequence patterns
    escape_sequence_regex = std::regex(R"(\x1b\[[0-9;]*[a-zA-Z])");
    color_escape_regex = std::regex(R"(\x1b\[([0-9;]+)m)");
    cursor_escape_regex = std::regex(R"(\x1b\[(\d+);(\d+)H)");
}

std::vector<ParsedLine> TerminalParser::parse_output(const std::string& output) {
    std::vector<ParsedLine> parsed_lines;
    std::vector<std::string> lines = utl::split_by_newline(output);
    
    for (const auto& line : lines) {
        ParsedLine parsed_line;
        parsed_line.content = strip_escape_sequences(line);
        parsed_line.type = LineType::UNKNOWN;
        parsed_line.attributes = current_attributes;
        
        // Detect line type
        if (is_prompt(parsed_line.content)) {
            parsed_line.type = LineType::PROMPT;
        } else if (is_error_output(parsed_line.content)) {
            parsed_line.type = LineType::ERROR_OUTPUT;
        } else if (is_command_output(parsed_line.content)) {
            parsed_line.type = LineType::COMMAND_OUTPUT;
        }
        
        // Check for escape sequences
        if (line.find("\x1b[") != std::string::npos) {
            parsed_line.is_escape_sequence = true;
            parsed_line.escape_sequence = line;
            parsed_line.attributes = parse_escape_sequence(line);
        }
        
        parsed_lines.push_back(parsed_line);
    }
    
    return parsed_lines;
}

std::string TerminalParser::strip_escape_sequences(const std::string& text) {
    std::string result = text;
    result = std::regex_replace(result, escape_sequence_regex, "");
    return result;
}

TerminalAttributes TerminalParser::parse_escape_sequence(const std::string& escape_seq) {
    TerminalAttributes attrs = current_attributes;
    
    std::smatch match;
    if (std::regex_search(escape_seq, match, color_escape_regex)) {
        std::string codes_str = match[1].str();
        std::istringstream iss(codes_str);
        std::string code_str;
        
        while (std::getline(iss, code_str, ';')) {
            int code = std::stoi(code_str);
            update_attributes_from_code(code);
        }
    }
    
    return attrs;
}

bool TerminalParser::is_prompt(const std::string& line) {
    for (const auto& pattern : prompt_patterns) {
        if (std::regex_search(line, pattern)) {
            return true;
        }
    }
    return false;
}

bool TerminalParser::is_command_output(const std::string& line) {
    // Simple heuristic: if it's not a prompt and not empty, it's likely command output
    return !line.empty() && !is_prompt(line) && !is_error_output(line);
}

bool TerminalParser::is_error_output(const std::string& line) {
    // Look for common error indicators
    std::vector<std::string> error_indicators = {
        "error:", "Error:", "ERROR:",
        "warning:", "Warning:", "WARNING:",
        "fatal:", "Fatal:", "FATAL:",
        "cannot", "Cannot", "CANNOT",
        "failed", "Failed", "FAILED",
        "not found", "Not found", "NOT FOUND"
    };
    
    for (const auto& indicator : error_indicators) {
        if (line.find(indicator) != std::string::npos) {
            return true;
        }
    }
    return false;
}

TerminalParser::CursorPosition TerminalParser::parse_cursor_escape(const std::string& escape_seq) {
    CursorPosition pos;
    std::smatch match;
    
    if (std::regex_search(escape_seq, match, cursor_escape_regex)) {
        pos.row = std::stoi(match[1].str());
        pos.col = std::stoi(match[2].str());
    }
    
    return pos;
}

void TerminalParser::clear_screen() {
    // Implementation for clearing screen
    current_cursor = {0, 0};
}

void TerminalParser::clear_line() {
    // Implementation for clearing current line
}

void TerminalParser::move_cursor(int row, int col) {
    current_cursor.row = row;
    current_cursor.col = col;
}

AnsiColor TerminalParser::parse_color_code(int code) {
    switch (code) {
        case 30: return AnsiColor::BLACK;
        case 31: return AnsiColor::RED;
        case 32: return AnsiColor::GREEN;
        case 33: return AnsiColor::YELLOW;
        case 34: return AnsiColor::BLUE;
        case 35: return AnsiColor::MAGENTA;
        case 36: return AnsiColor::CYAN;
        case 37: return AnsiColor::WHITE;
        case 90: return AnsiColor::BRIGHT_BLACK;
        case 91: return AnsiColor::BRIGHT_RED;
        case 92: return AnsiColor::BRIGHT_GREEN;
        case 93: return AnsiColor::BRIGHT_YELLOW;
        case 94: return AnsiColor::BRIGHT_BLUE;
        case 95: return AnsiColor::BRIGHT_MAGENTA;
        case 96: return AnsiColor::BRIGHT_CYAN;
        case 97: return AnsiColor::BRIGHT_WHITE;
        case 0: return AnsiColor::RESET;
        default: return AnsiColor::WHITE;
    }
}

void TerminalParser::update_attributes_from_code(int code) {
    switch (code) {
        case 0: // Reset
            current_attributes = TerminalAttributes{};
            break;
        case 1: // Bold
            current_attributes.bold = true;
            break;
        case 3: // Italic
            current_attributes.italic = true;
            break;
        case 4: // Underline
            current_attributes.underline = true;
            break;
        case 5: // Blink
            current_attributes.blink = true;
            break;
        case 7: // Reverse
            current_attributes.reverse = true;
            break;
        case 9: // Strikethrough
            current_attributes.strikethrough = true;
            break;
        default:
            if (code >= 30 && code <= 37) {
                current_attributes.foreground = parse_color_code(code);
            } else if (code >= 40 && code <= 47) {
                current_attributes.background = parse_color_code(code - 10);
            } else if (code >= 90 && code <= 97) {
                current_attributes.foreground = parse_color_code(code);
            } else if (code >= 100 && code <= 107) {
                current_attributes.background = parse_color_code(code - 10);
            }
            break;
    }
}
