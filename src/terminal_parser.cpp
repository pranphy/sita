#include "terminal_parser.h"
#include "utils.h"
#include <iostream>
#include <sstream>

TerminalParser::TerminalParser() {
  // Initialize prompt patterns
  prompt_patterns = {
      std::regex(R"(\$ $)"),                        // Basic prompt
      std::regex(R"(# $)"),                         // Root prompt
      std::regex(R"(\w+@\w+:\S+[#$] $)"),           // user@host:path$
      std::regex(R"(\w+@\w+:\S+> $)"),              // user@host:path>
      std::regex(R"(\w+@\w+:\S+\) $)"),             // user@host:path)
      std::regex(R"(\w+@\w+:\S+\[.*\]\$ $)"),       // user@host:path[git]$
      std::regex(R"(\w+@\w+:\S+\(.*\)\$ $)"),       // user@host:path(branch)$
      std::regex(R"(\w+@\w+:\S+\(.*\)\) $)"),       // user@host:path(branch))
      std::regex(R"(\w+@\w+:\S+\(.*\)> $)"),        // user@host:path(branch)>
      std::regex(R"(\w+@\w+:\S+\(.*\)\[.*\]\$ $)"), // Complex git prompt
      std::regex(R"([^
]*[$#>] $)")};

  // Initialize escape sequence patterns
  // Matches CSI ([\x1b[...]) and OSC ([\x1b]...])
  escape_sequence_regex =
      std::regex(R"(\x1b(\[[0-9;? ]*[a-zA-Z]|].*?(\x07|\x1b\\)))");
  color_escape_regex = std::regex(R"(\x1b\[([0-9;]+)m)");
  cursor_escape_regex = std::regex(R"(\x1b\[(\d+);(\d+)H)");
}

std::vector<ParsedLine>
TerminalParser::parse_output(const std::string &output) {
  std::vector<ParsedLine> parsed_lines;
  std::vector<std::string> lines = utl::split_by_newline(output);

  for (const auto &line : lines) {
    ParsedLine parsed_line;
    parsed_line.type = LineType::UNKNOWN;

    std::string full_content;
    size_t current_pos = 0;
    auto it =
        std::sregex_iterator(line.begin(), line.end(), escape_sequence_regex);
    auto end = std::sregex_iterator();

    // Start with current attributes
    TerminalAttributes line_attrs = current_attributes;

    while (it != end) {
      std::smatch match = *it;
      size_t escape_pos = match.position();

      // Add the text before the escape sequence as a segment
      if (escape_pos > current_pos) {
        std::string text_segment =
            line.substr(current_pos, escape_pos - current_pos);
        parsed_line.segments.push_back({text_segment, line_attrs});
        full_content += text_segment;
      }

      // Parse the escape sequence to update attributes
      std::string escape_seq = match.str();
      line_attrs = parse_escape_sequence(escape_seq);

      current_pos = escape_pos + match.length();
      it++;
    }

    // Add any remaining text after the last escape sequence
    if (current_pos < line.length()) {
      std::string remaining_text = line.substr(current_pos);
      parsed_line.segments.push_back({remaining_text, line_attrs});
      full_content += remaining_text;
    }

    // If there were no escape sequences, add the whole line as one segment
    if (parsed_line.segments.empty()) {
      parsed_line.segments.push_back({line, current_attributes});
      full_content = line;
    }

    // Detect line type based on the full, stripped content
    std::string stripped_content = strip_escape_sequences(full_content);
    if (is_prompt(stripped_content)) {
      parsed_line.type = LineType::PROMPT;
    } else if (is_error_output(stripped_content)) {
      parsed_line.type = LineType::ERROR_OUTPUT;
    } else {
      parsed_line.type = LineType::COMMAND_OUTPUT;
    }

    parsed_lines.push_back(parsed_line);
  }

  return parsed_lines;
}

std::string TerminalParser::strip_escape_sequences(const std::string &text) {
  std::string result = text;
  result = std::regex_replace(result, escape_sequence_regex, "");
  return result;
}

TerminalAttributes
TerminalParser::parse_escape_sequence(const std::string &escape_seq) {
  if (escape_seq.empty()) {
    return current_attributes;
  }

  std::smatch match;
  if (std::regex_search(escape_seq, match, color_escape_regex)) {
    std::string codes_str = match[1].str();
    std::istringstream iss(codes_str);
    std::string code_str;

    while (std::getline(iss, code_str, ';')) {
      if (!code_str.empty()) {
        int code = std::stoi(code_str);
        update_attributes_from_code(code);
      }
    }
  } else if (std::regex_search(escape_seq, match, cursor_escape_regex)) {
    CursorPosition pos = parse_cursor_escape(escape_seq);
    move_cursor(pos.row, pos.col);
  }

  return current_attributes;
}

bool TerminalParser::is_prompt(const std::string &line) {
  for (const auto &pattern : prompt_patterns) {
    if (std::regex_search(line, pattern)) {
      return true;
    }
  }
  return false;
}

bool TerminalParser::is_command_output(const std::string &line) {
  // Simple heuristic: if it's not a prompt and not empty, it's likely command
  // output
  return !line.empty() && !is_prompt(line) && !is_error_output(line);
}

bool TerminalParser::is_error_output(const std::string &line) {
  // Look for common error indicators
  std::vector<std::string> error_indicators = {
      "error:", "Error:", "ERROR:", "warning:",  "Warning:",  "WARNING:",
      "fatal:", "Fatal:", "FATAL:", "cannot",    "Cannot",    "CANNOT",
      "failed", "Failed", "FAILED", "not found", "Not found", "NOT FOUND"};

  for (const auto &indicator : error_indicators) {
    if (line.find(indicator) != std::string::npos) {
      return true;
    }
  }
  return false;
}

TerminalParser::CursorPosition
TerminalParser::parse_cursor_escape(const std::string &escape_seq) {
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
  case 30:
    return AnsiColor::BLACK;
  case 31:
    return AnsiColor::RED;
  case 32:
    return AnsiColor::GREEN;
  case 33:
    return AnsiColor::YELLOW;
  case 34:
    return AnsiColor::BLUE;
  case 35:
    return AnsiColor::MAGENTA;
  case 36:
    return AnsiColor::CYAN;
  case 37:
    return AnsiColor::WHITE;
  case 90:
    return AnsiColor::BRIGHT_BLACK;
  case 91:
    return AnsiColor::BRIGHT_RED;
  case 92:
    return AnsiColor::BRIGHT_GREEN;
  case 93:
    return AnsiColor::BRIGHT_YELLOW;
  case 94:
    return AnsiColor::BRIGHT_BLUE;
  case 95:
    return AnsiColor::BRIGHT_MAGENTA;
  case 96:
    return AnsiColor::BRIGHT_CYAN;
  case 97:
    return AnsiColor::BRIGHT_WHITE;
  case 0:
    return AnsiColor::RESET;
  default:
    return AnsiColor::WHITE;
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
