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
    std::vector<Cell> cells;
    int cursor_x = 0;
    bool line_clears_screen = false;

    // Process the line character by character (or chunk by chunk for escapes)
    size_t current_pos = 0;
    auto it =
        std::sregex_iterator(line.begin(), line.end(), escape_sequence_regex);
    auto end = std::sregex_iterator();

    while (current_pos < line.length()) {
      // Check if there's an escape sequence at current_pos
      if (it != end && (size_t)it->position() == current_pos) {
        std::string escape_seq = it->str();

        // Check for Erase in Line (EL) - \x1b[K or \x1b[nK
        std::regex el_regex(R"(\x1b\[(\d*)K)");
        std::smatch match;
        if (std::regex_search(escape_seq, match, el_regex)) {
          int mode = 0;
          if (match[1].length() > 0) {
            mode = std::stoi(match[1].str());
          }

          if (mode == 0) { // Erase from cursor to end of line
            if (cursor_x < (int)cells.size()) {
              cells.resize(cursor_x);
            }
          } else if (mode == 1) { // Erase from start to cursor
            for (int i = 0; i <= cursor_x && i < (int)cells.size(); ++i) {
              cells[i] = Cell{" ", current_attributes};
            }
          } else if (mode == 2) { // Erase entire line
            cells.clear();
            cursor_x = 0;
          }
        } else {
          // Check for Erase in Display (ED) - \x1b[J or \x1b[nJ
          std::regex ed_regex(R"(\x1b\[(\d*)J)");
          if (std::regex_search(escape_seq, match, ed_regex)) {
            int mode = 0;
            if (match[1].length() > 0) {
              mode = std::stoi(match[1].str());
            }
            if (mode == 2 || mode == 3) {
              // Clear screen/scrollback
              cells.clear();
              cursor_x = 0;
              line_clears_screen = true;
            }
          }
          // Other escape sequences (colors, cursor move, etc.)
          current_attributes = parse_escape_sequence(escape_seq);
        }

        current_pos += it->length();
        it++;
        continue;
      }

      // Check for next escape sequence position
      size_t next_escape_pos = (it != end) ? it->position() : line.length();

      // Process text up to next escape sequence
      while (current_pos < next_escape_pos) {
        char c = line[current_pos];

        if (c == '\b') {
          if (cursor_x > 0)
            cursor_x--;
        } else if (c == '\r') {
          cursor_x = 0;
        } else if (c == '\t') {
          // Tab stop every 8 spaces
          int next_tab = (cursor_x / 8 + 1) * 8;
          // Fill with spaces
          while (cursor_x < next_tab) {
            if (cursor_x >= (int)cells.size()) {
              cells.push_back(Cell{" ", current_attributes});
            } else {
              cells[cursor_x] = Cell{" ", current_attributes};
            }
            cursor_x++;
          }
        } else if ((unsigned char)c >= 32) { // Printable character
          std::string char_str(1, c);

          if (cursor_x >= (int)cells.size()) {
            cells.resize(cursor_x + 1);
          }
          cells[cursor_x] = Cell{char_str, current_attributes};
          cursor_x++;
        }
        // Ignore other control characters for now

        current_pos++;
      }
    }

    // Convert cells to segments
    ParsedLine parsed_line;
    parsed_line.type = LineType::UNKNOWN;
    parsed_line.clear_screen = line_clears_screen;

    if (cells.empty()) {
      // Empty line
      parsed_line.segments.push_back({"", current_attributes});
    } else {
      std::string current_content;
      TerminalAttributes last_attrs = cells[0].attributes;

      for (const auto &cell : cells) {
        // If attributes changed (and content is not empty), push segment
        // Note: empty cells (from resize) might have default attributes
        if (cell.content.empty())
          continue;

        // Simple comparison of attributes (we might need operator==)
        // For now, let's just check if memory matches or implement a helper
        // Or just push a new segment for every cell? No, that's inefficient.
        // Let's assume we can compare.
        bool attrs_changed =
            (cell.attributes.foreground != last_attrs.foreground ||
             cell.attributes.background != last_attrs.background ||
             cell.attributes.bold != last_attrs.bold); // etc...

        if (attrs_changed) {
          if (!current_content.empty()) {
            parsed_line.segments.push_back({current_content, last_attrs});
            current_content.clear();
          }
          last_attrs = cell.attributes;
        }
        current_content += cell.content;
      }
      if (!current_content.empty()) {
        parsed_line.segments.push_back({current_content, last_attrs});
      }
    }

    // Detect line type
    std::string full_content;
    for (const auto &seg : parsed_line.segments)
      full_content += seg.content;

    if (is_prompt(full_content)) {
      parsed_line.type = LineType::PROMPT;
    } else if (is_error_output(full_content)) {
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
  } else {
    // Check for Erase in Line (EL) - \x1b[K or \x1b[nK
    std::regex el_regex(R"(\x1b\[(\d*)K)");
    if (std::regex_search(escape_seq, match, el_regex)) {
      int mode = 0;
      if (match[1].length() > 0) {
        mode = std::stoi(match[1].str());
      }
      erase_in_line(mode);
    }

    // Check for Erase in Display (ED) - \x1b[J or \x1b[nJ
    std::regex ed_regex(R"(\x1b\[(\d*)J)");
    if (std::regex_search(escape_seq, match, ed_regex)) {
      int mode = 0;
      if (match[1].length() > 0) {
        mode = std::stoi(match[1].str());
      }
      erase_in_display(mode);
    }
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
  return !line.empty() && !is_prompt(line) && !is_error_output(line);
}

bool TerminalParser::is_error_output(const std::string &line) {
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

void TerminalParser::clear_screen() { current_cursor = {0, 0}; }

void TerminalParser::clear_line() {}

void TerminalParser::erase_in_line(int mode) {
  // We need to modify the current line being parsed.
  // Since we are parsing a string into cells, and 'cells' is local to the loop
  // in parse_output, we can't easily modify 'cells' from here directly without
  // changing the architecture. However, parse_output calls us to update
  // attributes.

  // Wait, parse_output iterates through the line and builds 'cells'.
  // If we encounter EL, we should modify 'cells'.
  // But 'cells' is not a member variable.

  // This architecture is slightly problematic for immediate side-effects on the
  // buffer. But wait, the escape sequence is processed inside the loop in
  // parse_output. We can't modify 'cells' from here because we don't have
  // access to it.

  // Alternative: parse_output should handle EL/ED logic directly or we pass
  // 'cells' to it? Or we make 'cells' a member variable (current_line_cells)?

  // For now, let's just log it or handle simple cases if possible.
  // Actually, the current architecture parses line by line.
  // If we have "abc\x1b[Kdef", it means "abc", then erase from cursor to end,
  // then "def". But "erase from cursor to end" implies we overwrite existing
  // cells with spaces? In the current 'parse_output' loop, we are building
  // 'cells' sequentially. We haven't built the "rest of the line" yet.

  // The issue is when we backspace, we move the cursor back.
  // Then we might emit \x1b[K to clear the character under cursor.
  // In 'parse_output', we handle backspace by moving 'cursor_x' back.
  // If we see \x1b[K, we should clear cells starting from 'cursor_x'.

  // So, we need to handle EL inside parse_output loop, similar to how we handle
  // \b.
}

void TerminalParser::erase_in_display(int mode) {
  // Similar issue. ED usually clears screen or part of it.
  // If mode=2, clear everything.
  if (mode == 2) {
    clear_screen();
  }
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
  case 0:
    current_attributes = TerminalAttributes{};
    break;
  case 1:
    current_attributes.bold = true;
    break;
  case 3:
    current_attributes.italic = true;
    break;
  case 4:
    current_attributes.underline = true;
    break;
  case 5:
    current_attributes.blink = true;
    break;
  case 7:
    current_attributes.reverse = true;
    break;
  case 9:
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
