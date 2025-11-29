#ifndef TERMINAL_PARSER_H
#define TERMINAL_PARSER_H

#include <map>
#include <regex>
#include <string>
#include <vector>

// Terminal line types for different rendering
enum class LineType {
  PROMPT,
  COMMAND_OUTPUT,
  ERROR_OUTPUT,
  USER_INPUT,
  UNKNOWN
};

// ANSI color codes
enum class AnsiColor {
  BLACK = 30,
  RED = 31,
  GREEN = 32,
  YELLOW = 33,
  BLUE = 34,
  MAGENTA = 35,
  CYAN = 36,
  WHITE = 37,
  BRIGHT_BLACK = 90,
  BRIGHT_RED = 91,
  BRIGHT_GREEN = 92,
  BRIGHT_YELLOW = 93,
  BRIGHT_BLUE = 94,
  BRIGHT_MAGENTA = 95,
  BRIGHT_CYAN = 96,
  BRIGHT_WHITE = 97,
  RESET = 0
};

// Terminal attributes
struct TerminalAttributes {
  AnsiColor foreground = AnsiColor::WHITE;
  AnsiColor background = AnsiColor::BLACK;
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool blink = false;
  bool reverse = false;
  bool strikethrough = false;
};

// Styled text segment
struct StyledSegment {
  std::string content;
  TerminalAttributes attributes;
};

// Parsed terminal line
struct ParsedLine {
  std::vector<StyledSegment> segments;
  LineType type;
  bool clear_screen = false;
};

struct Cell {
  std::string content;
  TerminalAttributes attributes;
};

class TerminalParser {
public:
  TerminalParser();
  std::vector<ParsedLine> parse_output(const std::string &output);
  std::string strip_escape_sequences(const std::string &text);
  bool is_prompt(const std::string &line);
  bool is_command_output(const std::string &line);
  bool is_error_output(const std::string &line);

private:
  // Common prompt patterns
  std::vector<std::regex> prompt_patterns;

  // ANSI escape sequence patterns
  std::regex escape_sequence_regex;
  std::regex color_escape_regex;
  std::regex cursor_escape_regex;

  // Current terminal state
  TerminalAttributes current_attributes;
  struct CursorPosition {
    int row;
    int col;
  };
  CursorPosition current_cursor;

  // Helper functions
  TerminalAttributes parse_escape_sequence(const std::string &escape_seq);
  CursorPosition parse_cursor_escape(const std::string &escape_seq);
  void clear_screen();
  void clear_line();
  void erase_in_line(int mode);
  void erase_in_display(int mode);
  void move_cursor(int row, int col);

  AnsiColor parse_color_code(int code);
  void update_attributes_from_code(int code);
};

#endif // TERMINAL_PARSER_H
